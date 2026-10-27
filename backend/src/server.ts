import express from 'express';
import cors from 'cors';
import path from 'path';
import apiRouter from './routes/api.js';
import { engineService } from './services/engineService.js';
import { storageService } from './services/storageService.js';

const app = express();
const PORT = process.env.PORT || 5000;

app.use(cors());
app.use(express.json({ limit: '10mb' }));
app.use(express.urlencoded({ extended: true, limit: '10mb' }));

// Periodic cleanup of temporary uploads (every 30 minutes)
setInterval(() => {
  storageService.cleanupOldJobs();
}, 30 * 60 * 1000);

// API routes
app.use('/api', apiRouter);

// Root greeting
app.get('/', (req, res) => {
  res.json({
    app: 'Smart File Compression Workbench API',
    status: 'online',
    engineAvailable: engineService.isAvailable()
  });
});

app.listen(PORT, () => {
  console.log(`[SFCW Backend] Running on http://localhost:${PORT}`);
  console.log(`[SFCW Backend] C++ Engine binary: ${engineService.getBinaryPath()}`);
  console.log(`[SFCW Backend] Engine status: ${engineService.isAvailable() ? 'READY' : 'NOT FOUND'}`);
});
