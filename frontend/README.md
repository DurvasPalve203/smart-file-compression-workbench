# Byteworks Compression Workbench

The frontend is a React and Vite application. Start the API and frontend in
separate terminals from the project root:

```powershell
cd backend
npm run dev
```

```powershell
cd frontend
npm run dev
```

Open the frontend URL printed by Vite (normally `http://localhost:5173`). The
backend should be available at `http://localhost:5000` and the C++ engine must
be present at `engine/bin/sfcw_engine.exe`.

To use a different backend host, set `VITE_API_URL` to its origin, for example
`http://localhost:5000`, before starting or building the frontend.
