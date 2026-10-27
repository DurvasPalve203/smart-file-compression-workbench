import { Router, Request, Response } from 'express';
import multer from 'multer';
import path from 'path';
import fs from 'fs';
import { v4 as uuidv4 } from 'uuid';
import { engineService } from '../services/engineService.js';
import { jobService } from '../services/jobService.js';
import { storageService } from '../services/storageService.js';
import { JobRecord } from '../types/index.js';

const router = Router();

// Configure multer for memory or temporary upload directory
const upload = multer({
  dest: path.join(storageService.getBaseUploadDir(), 'temp_uploads'),
  limits: {
    fileSize: 100 * 1024 * 1024 // 100 MB configurable limit
  }
});

// GET /api/health
router.get('/health', (req: Request, res: Response) => {
  const engineOk = engineService.isAvailable();
  res.json({
    status: engineOk ? 'ok' : 'degraded',
    version: '1.0.0',
    engineAvailable: engineOk,
    enginePath: engineService.getBinaryPath(),
    uptimeSeconds: process.uptime()
  });
});

// GET /api/history
router.get('/history', (req: Request, res: Response) => {
  const jobs = jobService.getAllJobs();
  const stats = jobService.getSessionStats();
  res.json({
    jobs,
    stats
  });
});

// POST /api/history/clear
router.post('/history/clear', (req: Request, res: Response) => {
  jobService.clearHistory();
  res.json({ success: true, message: 'History cleared' });
});

// GET /api/history/export
router.get('/history/export', (req: Request, res: Response) => {
  const format = req.query.format === 'csv' ? 'csv' : 'json';
  const jobs = jobService.getAllJobs();

  if (format === 'csv') {
    let csv = 'JobId,Type,Status,CreatedAt,Filename,Algorithm,OriginalSize,CompressedSize,SpaceSavedPercent,CompressionRatio,DurationMs\n';
    for (const j of jobs) {
      csv += `"${j.jobId}","${j.type}","${j.status}","${j.createdAt}","${j.filename || ''}","${j.algorithm || ''}",${j.originalSize || 0},${j.compressedSize || 0},${j.spaceSavedPercent || 0},${j.compressionRatio || 0},${j.durationMs || 0}\n`;
    }
    res.setHeader('Content-Type', 'text/csv');
    res.setHeader('Content-Disposition', 'attachment; filename="sfcw_benchmark_report.csv"');
    res.send(csv);
  } else {
    res.setHeader('Content-Type', 'application/json');
    res.setHeader('Content-Disposition', 'attachment; filename="sfcw_summary_report.json"');
    res.json({
      exportedAt: new Date().toISOString(),
      sessionStats: jobService.getSessionStats(),
      jobs
    });
  }
});

// POST /api/files/analyze
router.post('/files/analyze', upload.single('file'), async (req: Request, res: Response) => {
  try {
    if (!req.file) {
      res.status(400).json({ error: 'No file uploaded' });
      return;
    }

    const jobId = uuidv4();
    const jobDir = storageService.createJobDirectory(jobId);
    const originalFilename = storageService.sanitizeFilename(req.file.originalname);
    const inputFilePath = path.join(jobDir, originalFilename);

    fs.copyFileSync(req.file.path, inputFilePath);
    fs.unlinkSync(req.file.path); // remove temp multer file

    const analysis = await engineService.runAnalyze(inputFilePath);

    res.json({
      jobId,
      filename: req.file.originalname,
      originalSize: req.file.size,
      analysis,
      supportedAlgorithms: [
        { id: 'huffman', name: 'Huffman Coding', category: 'Greedy Frequency Coding' },
        { id: 'lzw', name: 'LZW Compression', category: 'Dictionary-Based' },
        { id: 'rle', name: 'Run-Length Encoding', category: 'Repetition Run' }
      ]
    });
  } catch (error: any) {
    res.status(500).json({ error: error.message || 'File analysis failed' });
  }
});

// POST /api/compress
router.post('/compress', upload.single('file'), async (req: Request, res: Response) => {
  try {
    let inputFilePath = '';
    let originalFilename = '';
    let originalSize = 0;
    const jobId = uuidv4();
    const jobDir = storageService.createJobDirectory(jobId);

    if (req.file) {
      originalFilename = storageService.sanitizeFilename(req.file.originalname);
      originalSize = req.file.size;
      inputFilePath = path.join(jobDir, originalFilename);
      fs.copyFileSync(req.file.path, inputFilePath);
      fs.unlinkSync(req.file.path);
    } else if (req.body.existingJobId && req.body.existingFilename) {
      const prevDir = storageService.getJobDirectory(req.body.existingJobId);
      inputFilePath = path.join(prevDir, storageService.sanitizeFilename(req.body.existingFilename));
      if (!fs.existsSync(inputFilePath)) {
        res.status(404).json({ error: 'Referenced existing file not found' });
        return;
      }
      originalFilename = req.body.existingFilename;
      originalSize = fs.statSync(inputFilePath).size;
    } else {
      res.status(400).json({ error: 'No file or existing upload specified' });
      return;
    }

    const algorithm = (req.body.algorithm || 'huffman').toLowerCase();
    const enableTrace = req.body.trace === 'true' || req.body.trace === true;

    const outputFilename = `${originalFilename}.${algorithm}.sfc`;
    const outputFilePath = path.join(jobDir, outputFilename);
    const traceFilePath = path.join(jobDir, 'trace.json');

    const jobRecord: JobRecord = {
      jobId,
      type: 'compress',
      status: 'processing',
      createdAt: new Date().toISOString(),
      filename: originalFilename,
      originalSize,
      algorithm
    };
    jobService.createJob(jobRecord);

    const result = await engineService.runCompress(
      algorithm,
      inputFilePath,
      outputFilePath,
      enableTrace,
      traceFilePath
    );

    const compressedSize = fs.statSync(outputFilePath).size;
    const hasTrace = fs.existsSync(traceFilePath);

    let traceData: any = null;
    if (hasTrace) {
      try {
        traceData = JSON.parse(fs.readFileSync(traceFilePath, 'utf-8'));
      } catch (err) {
        console.error('Error reading trace JSON:', err);
      }
    }

    jobService.updateJob(jobId, {
      status: 'completed',
      compressedSize,
      spaceSavedPercent: result.spaceSavedPercent,
      compressionRatio: result.compressionRatio,
      durationMs: result.compressionTimeMs,
      downloadUrl: `/api/jobs/${jobId}/download`,
      downloadFilename: outputFilename,
      result,
      hasTrace
    });

    res.json({
      jobId,
      filename: originalFilename,
      algorithmName: result.algorithmName,
      algorithmSlug: result.algorithmSlug,
      originalSize: result.originalSize,
      compressedSize,
      payloadSize: result.payloadSize,
      metadataSize: result.metadataSize,
      compressionRatio: result.compressionRatio,
      spaceSavedPercent: result.spaceSavedPercent,
      compressionTimeMs: result.compressionTimeMs,
      decompressionTimeMs: result.decompressionTimeMs,
      compressionThroughput: result.compressionThroughput,
      decompressionThroughput: result.decompressionThroughput,
      integrityVerified: result.integrityVerified,
      originalCrc32: result.originalCrc32,
      restoredCrc32: result.restoredCrc32,
      theoreticalTimeComplexity: result.theoreticalTimeComplexity,
      theoreticalSpaceComplexity: result.theoreticalSpaceComplexity,
      downloadUrl: `/api/jobs/${jobId}/download`,
      downloadFilename: outputFilename,
      traceUrl: hasTrace ? `/api/jobs/${jobId}/trace` : undefined,
      trace: traceData
    });
  } catch (error: any) {
    res.status(500).json({ error: error.message || 'Compression failed' });
  }
});

// POST /api/benchmark
router.post('/benchmark', upload.single('file'), async (req: Request, res: Response) => {
  try {
    let inputFilePath = '';
    let originalFilename = '';
    let originalSize = 0;
    const jobId = uuidv4();
    const jobDir = storageService.createJobDirectory(jobId);

    if (req.file) {
      originalFilename = storageService.sanitizeFilename(req.file.originalname);
      originalSize = req.file.size;
      inputFilePath = path.join(jobDir, originalFilename);
      fs.copyFileSync(req.file.path, inputFilePath);
      fs.unlinkSync(req.file.path);
    } else if (req.body.existingJobId && req.body.existingFilename) {
      const prevDir = storageService.getJobDirectory(req.body.existingJobId);
      inputFilePath = path.join(prevDir, storageService.sanitizeFilename(req.body.existingFilename));
      if (!fs.existsSync(inputFilePath)) {
        res.status(404).json({ error: 'Referenced existing file not found' });
        return;
      }
      originalFilename = req.body.existingFilename;
      originalSize = fs.statSync(inputFilePath).size;
    } else {
      res.status(400).json({ error: 'No file or existing upload specified' });
      return;
    }

    let algorithms = ['huffman', 'lzw', 'rle'];
    if (req.body.algorithms) {
      if (Array.isArray(req.body.algorithms)) {
        algorithms = req.body.algorithms;
      } else if (typeof req.body.algorithms === 'string') {
        algorithms = req.body.algorithms.split(',').map((s: string) => s.trim().toLowerCase());
      }
    }

    const trials = req.body.trials ? parseInt(req.body.trials, 10) : 3;

    const jobRecord: JobRecord = {
      jobId,
      type: 'benchmark',
      status: 'processing',
      createdAt: new Date().toISOString(),
      filename: originalFilename,
      originalSize
    };
    jobService.createJob(jobRecord);

    const benchmarkResult = await engineService.runBenchmark(inputFilePath, algorithms, trials);

    jobService.updateJob(jobId, {
      status: 'completed',
      result: benchmarkResult
    });

    res.json({
      jobId,
      filename: originalFilename,
      originalSize,
      ...benchmarkResult
    });
  } catch (error: any) {
    res.status(500).json({ error: error.message || 'Benchmark failed' });
  }
});

// POST /api/decompress
router.post('/decompress', upload.single('file'), async (req: Request, res: Response) => {
  try {
    let inputFilePath = '';
    let originalFilename = '';
    const jobId = uuidv4();
    const jobDir = storageService.createJobDirectory(jobId);

    if (req.file) {
      originalFilename = storageService.sanitizeFilename(req.file.originalname);
      inputFilePath = path.join(jobDir, originalFilename);
      fs.copyFileSync(req.file.path, inputFilePath);
      fs.unlinkSync(req.file.path);
    } else if (req.body.existingJobId) {
      const prevDir = storageService.getJobDirectory(req.body.existingJobId);
      const files = fs.readdirSync(prevDir).filter(f => f.endsWith('.sfc'));
      if (files.length === 0) {
        res.status(404).json({ error: 'No compressed .sfc file found in referenced job' });
        return;
      }
      originalFilename = files[0];
      inputFilePath = path.join(prevDir, originalFilename);
    } else {
      res.status(400).json({ error: 'No compressed file uploaded' });
      return;
    }

    // Determine restored output filename (strip .sfc and .algo if present)
    let restoredFilename = originalFilename.replace(/\.sfc$/, '');
    restoredFilename = restoredFilename.replace(/\.(huffman|lzw|rle)$/, '');
    if (restoredFilename === originalFilename || restoredFilename.length === 0) {
      restoredFilename = `restored_${originalFilename}`;
    }

    const outputFilePath = path.join(jobDir, restoredFilename);

    const jobRecord: JobRecord = {
      jobId,
      type: 'decompress',
      status: 'processing',
      createdAt: new Date().toISOString(),
      filename: originalFilename,
      originalSize: fs.statSync(inputFilePath).size
    };
    jobService.createJob(jobRecord);

    const result = await engineService.runDecompress(inputFilePath, outputFilePath);

    jobService.updateJob(jobId, {
      status: 'completed',
      downloadUrl: `/api/jobs/${jobId}/download`,
      downloadFilename: restoredFilename,
      result
    });

    res.json({
      jobId,
      originalCompressedFilename: originalFilename,
      restoredFilename,
      algorithmName: result.algorithmName,
      algorithmSlug: result.algorithmSlug,
      originalSize: result.originalSize,
      restoredSize: result.restoredSize,
      decompressionTimeMs: result.decompressionTimeMs,
      decompressionThroughput: result.decompressionThroughput,
      originalCrc32: result.originalCrc32,
      restoredCrc32: result.restoredCrc32,
      integrityVerified: result.integrityVerified,
      downloadUrl: `/api/jobs/${jobId}/download`
    });
  } catch (error: any) {
    res.status(500).json({ error: error.message || 'Decompression failed' });
  }
});

// GET /api/jobs/:jobId
router.get('/jobs/:jobId', (req: Request, res: Response) => {
  const jobId = Array.isArray(req.params.jobId) ? req.params.jobId[0] : req.params.jobId;
  const job = jobService.getJob(jobId);
  if (!job) {
    res.status(404).json({ error: 'Job not found' });
    return;
  }
  res.json(job);
});

// GET /api/jobs/:jobId/download
router.get('/jobs/:jobId/download', (req: Request, res: Response) => {
  const jobId = Array.isArray(req.params.jobId) ? req.params.jobId[0] : req.params.jobId;
  const job = jobService.getJob(jobId);
  if (!job) {
    res.status(404).json({ error: 'Job not found' });
    return;
  }

  const jobDir = storageService.getJobDirectory(jobId);
  const downloadFilename = job.downloadFilename || 'download.bin';
  const filePath = path.join(jobDir, downloadFilename);

  if (!fs.existsSync(filePath)) {
    res.status(404).json({ error: 'File artifact not found' });
    return;
  }

  res.setHeader('Content-Disposition', `attachment; filename="${downloadFilename}"`);
  res.setHeader('Content-Type', 'application/octet-stream');
  const stream = fs.createReadStream(filePath);
  stream.pipe(res);
});

// GET /api/jobs/:jobId/trace
router.get('/jobs/:jobId/trace', (req: Request, res: Response) => {
  const jobId = Array.isArray(req.params.jobId) ? req.params.jobId[0] : req.params.jobId;
  const jobDir = storageService.getJobDirectory(jobId);
  const traceFilePath = path.join(jobDir, 'trace.json');

  if (!fs.existsSync(traceFilePath)) {
    res.status(404).json({ error: 'Trace data not found for this job' });
    return;
  }

  try {
    const trace = JSON.parse(fs.readFileSync(traceFilePath, 'utf-8'));
    res.json(trace);
  } catch (err: any) {
    res.status(500).json({ error: 'Failed to read trace data' });
  }
});

export default router;
