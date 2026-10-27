import path from 'path';
import fs from 'fs';
import { execFile } from 'child_process';
import { promisify } from 'util';
import { AlgorithmResult, BenchmarkResponse, FileAnalysis } from '../types/index.js';

const execFileAsync = promisify(execFile);

export class EngineService {
  private engineBinaryPath: string;

  constructor() {
    // Check multiple potential locations
    const candidatePaths = [
      path.resolve(__dirname, '../../../engine/bin/sfcw_engine.exe'),
      path.resolve(__dirname, '../../engine/bin/sfcw_engine.exe'),
      path.resolve(process.cwd(), 'engine/bin/sfcw_engine.exe'),
      path.resolve(process.cwd(), '../engine/bin/sfcw_engine.exe')
    ];

    const found = candidatePaths.find(p => fs.existsSync(p));
    if (found) {
      this.engineBinaryPath = found;
    } else {
      this.engineBinaryPath = candidatePaths[0];
    }
  }

  public getBinaryPath(): string {
    return this.engineBinaryPath;
  }

  public isAvailable(): boolean {
    return fs.existsSync(this.engineBinaryPath);
  }

  public async runAnalyze(inputPath: string): Promise<FileAnalysis> {
    if (!this.isAvailable()) {
      throw new Error(`Compression engine binary not found at: ${this.engineBinaryPath}`);
    }

    const args = ['--action', 'analyze', '--input', inputPath];
    const { stdout, stderr } = await execFileAsync(this.engineBinaryPath, args, {
      timeout: 30000,
      maxBuffer: 10 * 1024 * 1024
    });

    if (stderr && stderr.includes('"error"')) {
      const errObj = JSON.parse(stderr);
      throw new Error(errObj.error || stderr);
    }

    return JSON.parse(stdout) as FileAnalysis;
  }

  public async runCompress(
    algo: string,
    inputPath: string,
    outputPath: string,
    enableTrace = false,
    tracePath?: string
  ): Promise<AlgorithmResult> {
    if (!this.isAvailable()) {
      throw new Error(`Compression engine binary not found at: ${this.engineBinaryPath}`);
    }

    const args = [
      '--action', 'compress',
      '--algo', algo,
      '--input', inputPath,
      '--output', outputPath
    ];

    if (enableTrace && tracePath) {
      args.push('--trace-file', tracePath);
    }

    const { stdout, stderr } = await execFileAsync(this.engineBinaryPath, args, {
      timeout: 60000,
      maxBuffer: 10 * 1024 * 1024
    });

    if (stderr && stderr.includes('"error"')) {
      const errObj = JSON.parse(stderr);
      throw new Error(errObj.error || stderr);
    }

    return JSON.parse(stdout) as AlgorithmResult;
  }

  public async runDecompress(inputPath: string, outputPath: string): Promise<any> {
    if (!this.isAvailable()) {
      throw new Error(`Compression engine binary not found at: ${this.engineBinaryPath}`);
    }

    const args = [
      '--action', 'decompress',
      '--input', inputPath,
      '--output', outputPath
    ];

    const { stdout, stderr } = await execFileAsync(this.engineBinaryPath, args, {
      timeout: 60000,
      maxBuffer: 10 * 1024 * 1024
    });

    if (stderr && stderr.includes('"error"')) {
      const errObj = JSON.parse(stderr);
      throw new Error(errObj.error || stderr);
    }

    return JSON.parse(stdout);
  }

  public async runBenchmark(
    inputPath: string,
    algos: string[] = ['huffman', 'lzw', 'rle'],
    trials = 3
  ): Promise<BenchmarkResponse> {
    if (!this.isAvailable()) {
      throw new Error(`Compression engine binary not found at: ${this.engineBinaryPath}`);
    }

    const args = [
      '--action', 'benchmark',
      '--input', inputPath,
      '--trials', trials.toString(),
      '--algos', algos.join(',')
    ];

    const { stdout, stderr } = await execFileAsync(this.engineBinaryPath, args, {
      timeout: 120000,
      maxBuffer: 20 * 1024 * 1024
    });

    if (stderr && stderr.includes('"error"')) {
      const errObj = JSON.parse(stderr);
      throw new Error(errObj.error || stderr);
    }

    return JSON.parse(stdout) as BenchmarkResponse;
  }
}

export const engineService = new EngineService();
