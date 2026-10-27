# Smart File Compression Workbench

A local file compression workbench with a React frontend, TypeScript/Express
API, and C++ compression engine. Supports Huffman coding, LZW, and run-length
encoding, plus file analysis, benchmarking, and decompression.

## Requirements

- Node.js 20.19+ or 22.12+
- npm
- Windows prebuilt engine at `engine/bin/sfcw_engine.exe`, or a C++17 compiler
  such as MinGW-w64 g++ to build it from source

## Run locally

In separate terminals, from the project root:

```powershell
cd backend
npm ci
npm run dev
```

```powershell
cd frontend
npm ci
npm run dev
```

Open the URL Vite prints (usually `http://localhost:5173`). The backend health
endpoint is `http://localhost:5000/api/health`.

The backend expects the engine at `engine/bin/sfcw_engine.exe`. If rebuilding
the C++ engine on Windows, run `engine\build.bat` from the `engine` directory
in a terminal where `g++` is available.

To use another backend origin, set `VITE_API_URL` before starting or building
the frontend (for example, `http://localhost:5000`).

## Build and lint

```powershell
cd backend
npm run build
```

```powershell
cd frontend
npm run build
npm run lint
```

Uploaded files and generated build output are intentionally excluded from Git.
