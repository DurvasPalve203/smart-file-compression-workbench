export interface ByteFrequency {
  byte: number;
  count: number;
}

export interface FileAnalysis {
  totalBytes: number;
  distinctSymbols: number;
  entropy: number;
  totalRuns: number;
  avgRunLength: number;
  repetitionRatio: number;
  recommendedAlgorithm: string;
  recommendationReason: string;
  topFrequencies: ByteFrequency[];
}

export interface AlgorithmResult {
  algorithmName: string;
  algorithmSlug: string;
  originalSize: number;
  compressedSize: number;
  payloadSize: number;
  metadataSize: number;
  compressionRatio: number;
  spaceSavedPercent: number;
  compressionTimeMs: number;
  decompressionTimeMs: number;
  compressionThroughput: number;
  decompressionThroughput: number;
  integrityVerified: boolean;
  originalCrc32: number;
  restoredCrc32: number;
  theoreticalTimeComplexity: string;
  theoreticalSpaceComplexity: string;
}

export interface BenchmarkResponse {
  inputAnalysis: FileAnalysis;
  trials: number;
  empiricalWinner: string;
  empiricalReason: string;
  results: AlgorithmResult[];
}

export type JobType = 'compress' | 'decompress' | 'benchmark';
export type JobStatus = 'queued' | 'processing' | 'completed' | 'failed';

export interface JobRecord {
  jobId: string;
  type: JobType;
  status: JobStatus;
  createdAt: string;
  filename: string;
  originalSize: number;
  algorithm?: string;
  compressedSize?: number;
  spaceSavedPercent?: number;
  compressionRatio?: number;
  durationMs?: number;
  downloadUrl?: string;
  downloadFilename?: string;
  error?: string;
  result?: any;
  hasTrace?: boolean;
}

export interface SessionStats {
  totalJobs: number;
  totalOriginalBytes: number;
  totalCompressedBytes: number;
  totalBytesSaved: number;
  avgSpaceSavingPercent: number;
  algorithmUsage: Record<string, number>;
}
