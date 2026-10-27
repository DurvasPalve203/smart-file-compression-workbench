import { JobRecord, SessionStats } from '../types/index.js';

export class JobService {
  private jobs: Map<string, JobRecord> = new Map();

  public createJob(record: JobRecord): void {
    this.jobs.set(record.jobId, record);
  }

  public getJob(jobId: string): JobRecord | undefined {
    return this.jobs.get(jobId);
  }

  public updateJob(jobId: string, updates: Partial<JobRecord>): void {
    const existing = this.jobs.get(jobId);
    if (existing) {
      this.jobs.set(jobId, { ...existing, ...updates });
    }
  }

  public getAllJobs(): JobRecord[] {
    return Array.from(this.jobs.values()).sort(
      (a, b) => new Date(b.createdAt).getTime() - new Date(a.createdAt).getTime()
    );
  }

  public getSessionStats(): SessionStats {
    let totalJobs = 0;
    let totalOriginalBytes = 0;
    let totalCompressedBytes = 0;
    let totalSaved = 0;
    let sumSpaceSavingPercent = 0;
    let completedCompressJobs = 0;
    const algorithmUsage: Record<string, number> = {
      huffman: 0,
      lzw: 0,
      rle: 0
    };

    for (const job of this.jobs.values()) {
      if (job.status === 'completed' && job.type === 'compress') {
        totalJobs++;
        completedCompressJobs++;
        totalOriginalBytes += job.originalSize || 0;
        totalCompressedBytes += job.compressedSize || 0;
        if (job.compressedSize && job.originalSize && job.originalSize > job.compressedSize) {
          totalSaved += (job.originalSize - job.compressedSize);
        }
        if (job.spaceSavedPercent !== undefined) {
          sumSpaceSavingPercent += job.spaceSavedPercent;
        }
        if (job.algorithm) {
          const key = job.algorithm.toLowerCase();
          algorithmUsage[key] = (algorithmUsage[key] || 0) + 1;
        }
      }
    }

    const avgSpaceSavingPercent = completedCompressJobs > 0 
      ? Number((sumSpaceSavingPercent / completedCompressJobs).toFixed(2)) 
      : 0;

    return {
      totalJobs,
      totalOriginalBytes,
      totalCompressedBytes,
      totalBytesSaved: totalSaved,
      avgSpaceSavingPercent,
      algorithmUsage
    };
  }

  public clearHistory(): void {
    this.jobs.clear();
  }
}

export const jobService = new JobService();
