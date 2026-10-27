import path from 'path';
import fs from 'fs';
import { v4 as uuidv4 } from 'uuid';

export class StorageService {
  private baseUploadDir: string;

  constructor() {
    this.baseUploadDir = path.resolve(__dirname, '../../uploads');
    if (!fs.existsSync(this.baseUploadDir)) {
      fs.mkdirSync(this.baseUploadDir, { recursive: true });
    }
  }

  public getBaseUploadDir(): string {
    return this.baseUploadDir;
  }

  public createJobDirectory(jobId: string): string {
    const jobDir = path.join(this.baseUploadDir, jobId);
    if (!fs.existsSync(jobDir)) {
      fs.mkdirSync(jobDir, { recursive: true });
    }
    return jobDir;
  }

  public getJobDirectory(jobId: string): string {
    // Basic sanitization
    const sanitizedJobId = path.basename(jobId);
    return path.join(this.baseUploadDir, sanitizedJobId);
  }

  public sanitizeFilename(filename: string): string {
    return filename.replace(/[^a-zA-Z0-9._-]/g, '_');
  }

  public cleanupOldJobs(maxAgeMs = 1000 * 60 * 60 * 2): void {
    try {
      const now = Date.now();
      const entries = fs.readdirSync(this.baseUploadDir, { withFileTypes: true });
      for (const entry of entries) {
        if (entry.isDirectory()) {
          const dirPath = path.join(this.baseUploadDir, entry.name);
          const stat = fs.statSync(dirPath);
          if (now - stat.mtimeMs > maxAgeMs) {
            fs.rmSync(dirPath, { recursive: true, force: true });
          }
        }
      }
    } catch (err) {
      console.error('Failed to cleanup old jobs:', err);
    }
  }
}

export const storageService = new StorageService();
