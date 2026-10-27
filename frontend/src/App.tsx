import { useCallback, useEffect, useState } from 'react'
import './App.css'

const API_ROOT = import.meta.env.VITE_API_URL ?? 'http://localhost:5000'

type JsonRecord = Record<string, unknown>
type Job = {
  jobId: string
  type: string
  status: string
  createdAt: string
  filename: string
  originalSize: number
  algorithm?: string
  compressedSize?: number
  spaceSavedPercent?: number
  downloadUrl?: string
  error?: string
}

type Health = {
  status: string
  engineAvailable: boolean
}

const algorithms = [
  { id: 'huffman', name: 'Huffman coding', description: 'Frequency-based prefix coding' },
  { id: 'lzw', name: 'LZW', description: 'Dictionary-based compression' },
  { id: 'rle', name: 'Run-length encoding', description: 'Best for repeated sequences' },
]

const tabs = ['Compress', 'Analyze', 'Benchmark', 'Decompress', 'History'] as const
type Tab = (typeof tabs)[number]

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(`${API_ROOT}/api${path}`, init)
  const payload: unknown = await response.json().catch(() => ({}))
  if (!response.ok) {
    const message = isRecord(payload) && typeof payload.error === 'string'
      ? payload.error
      : `Request failed (${response.status})`
    throw new Error(message)
  }
  return payload as T
}

function isRecord(value: unknown): value is JsonRecord {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}

function formatBytes(bytes: number | undefined): string {
  if (bytes === undefined || !Number.isFinite(bytes)) return '—'
  if (bytes < 1024) return `${bytes} B`
  const units = ['KB', 'MB', 'GB', 'TB']
  let value = bytes / 1024
  let unit = 0
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024
    unit += 1
  }
  return `${value.toFixed(1)} ${units[unit]}`
}

function labelFor(key: string): string {
  return key.replace(/([A-Z])/g, ' $1').replace(/^./, (letter) => letter.toUpperCase())
}

function displayValue(value: unknown): string {
  if (typeof value === 'boolean') return value ? 'Verified' : 'Not verified'
  if (typeof value === 'number') return Number.isInteger(value) ? value.toLocaleString() : value.toFixed(2)
  if (typeof value === 'string') return value
  if (value === null || value === undefined) return '—'
  return JSON.stringify(value)
}

function App() {
  const [tab, setTab] = useState<Tab>('Compress')
  const [file, setFile] = useState<File | null>(null)
  const [algorithm, setAlgorithm] = useState('huffman')
  const [trace, setTrace] = useState(false)
  const [health, setHealth] = useState<Health | null>(null)
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState('')
  const [result, setResult] = useState<JsonRecord | null>(null)
  const [jobs, setJobs] = useState<Job[]>([])

  const refreshJobs = useCallback(async () => {
    try {
      const history = await request<{ jobs: Job[] }>('/history')
      setJobs(history.jobs)
    } catch {
      setJobs([])
    }
  }, [])

  useEffect(() => {
    let active = true
    void request<Health>('/health').then(
      (status) => { if (active) setHealth(status) },
      () => { if (active) setHealth(null) },
    )
    void request<{ jobs: Job[] }>('/history').then(
      (history) => { if (active) setJobs(history.jobs) },
      () => { if (active) setJobs([]) },
    )
    return () => { active = false }
  }, [])

  function makeFormData(includeAlgorithm = false): FormData {
    const form = new FormData()
    if (file) form.append('file', file)
    if (includeAlgorithm) form.append('algorithm', algorithm)
    return form
  }

  async function runAction(action: 'compress' | 'analyze' | 'benchmark' | 'decompress') {
    if (!file) {
      setError('Choose a file before starting.')
      return
    }
    setBusy(true)
    setError('')
    setResult(null)
    try {
      let payload: JsonRecord
      if (action === 'compress') {
        const form = makeFormData(true)
        form.append('trace', String(trace))
        payload = await request<JsonRecord>('/compress', { method: 'POST', body: form })
      } else if (action === 'analyze') {
        payload = await request<JsonRecord>('/files/analyze', { method: 'POST', body: makeFormData() })
      } else if (action === 'benchmark') {
        payload = await request<JsonRecord>('/benchmark', { method: 'POST', body: makeFormData() })
      } else {
        payload = await request<JsonRecord>('/decompress', { method: 'POST', body: makeFormData() })
      }
      setResult(payload)
      await refreshJobs()
    } catch (actionError) {
      setError(actionError instanceof Error ? actionError.message : 'The request could not be completed.')
    } finally {
      setBusy(false)
    }
  }

  async function clearHistory() {
    setError('')
    try {
      await request('/history/clear', { method: 'POST' })
      await refreshJobs()
    } catch (actionError) {
      setError(actionError instanceof Error ? actionError.message : 'Could not clear history.')
    }
  }

  const resultRows = result
    ? Object.entries(result).filter(([key, value]) => (
      !['jobId', 'filename', 'downloadUrl', 'downloadFilename', 'trace'].includes(key)
      && !Array.isArray(value)
      && !isRecord(value)
    ))
    : []
  const benchmarkResults = result && Array.isArray(result.results)
    ? result.results.filter(isRecord)
    : []
  const analysis = result && isRecord(result.analysis) ? result.analysis : null
  const downloadUrl = result && typeof result.downloadUrl === 'string' ? result.downloadUrl : null

  return (
    <main className="app-shell">
      <header className="topbar">
        <a className="brand" href="/" aria-label="SFCW home">
          <span className="brand-mark">S</span>
          <span>BYTE<span className="brand-light">WORKS</span></span>
        </a>
        <div className={`service-status ${health?.status === 'ok' && health.engineAvailable ? 'online' : 'offline'}`}>
          <span className="status-dot" />
          {health?.status === 'ok' && health.engineAvailable ? 'Engine online' : 'Engine unavailable'}
        </div>
      </header>

      <section className="intro">
        <p className="eyebrow">SMART FILE COMPRESSION WORKBENCH</p>
        <h1>Make every byte <span>count.</span></h1>
        <p className="intro-copy">Compress, inspect, and benchmark files with classic algorithms — all in one place.</p>
      </section>

      <nav className="tabs" aria-label="Workbench tools">
        {tabs.map((item) => (
          <button key={item} className={tab === item ? 'tab active' : 'tab'} onClick={() => {
            setTab(item)
            setResult(null)
            setError('')
          }}>
            {item}
          </button>
        ))}
      </nav>

      <section className="workspace">
        {tab === 'History' ? (
          <div className="history-panel">
            <div className="panel-heading">
              <div><p className="eyebrow">YOUR SESSION</p><h2>Recent jobs</h2></div>
              <button className="button button-quiet" onClick={() => void clearHistory()} disabled={jobs.length === 0}>Clear history</button>
            </div>
            {jobs.length === 0 ? <p className="empty-state">Your completed work will appear here.</p> : (
              <div className="table-scroll">
                <table>
                  <thead><tr><th>File</th><th>Operation</th><th>Status</th><th>Original</th><th>Saved</th><th /></tr></thead>
                  <tbody>{jobs.map((job) => (
                    <tr key={job.jobId}>
                      <td className="file-cell">{job.filename}</td>
                      <td>{job.type}{job.algorithm ? ` · ${job.algorithm}` : ''}</td>
                      <td><span className={`job-status ${job.status}`}>{job.status}</span></td>
                      <td>{formatBytes(job.originalSize)}</td>
                      <td>{job.spaceSavedPercent === undefined ? '—' : `${job.spaceSavedPercent.toFixed(1)}%`}</td>
                      <td>{job.downloadUrl && <a className="table-link" href={`${API_ROOT}${job.downloadUrl}`}>Download</a>}</td>
                    </tr>
                  ))}</tbody>
                </table>
              </div>
            )}
          </div>
        ) : (
          <>
            <div className="tool-panel">
              <div className="panel-heading">
                <div>
                  <p className="eyebrow">{tab === 'Compress' ? 'REDUCE FILE SIZE' : tab === 'Analyze' ? 'UNDERSTAND YOUR DATA' : tab === 'Benchmark' ? 'COMPARE ALGORITHMS' : 'RESTORE A FILE'}</p>
                  <h2>{tab === 'Compress' ? 'Compress a file' : tab === 'Analyze' ? 'Analyze a file' : tab === 'Benchmark' ? 'Run a benchmark' : 'Decompress a file'}</h2>
                </div>
                <span className="step-count">01 <span>/ 02</span></span>
              </div>

              <label className="upload-zone" htmlFor="file-input">
                <input id="file-input" type="file" onChange={(event) => {
                  setFile(event.target.files?.[0] ?? null)
                  setResult(null)
                  setError('')
                }} />
                <span className="upload-icon" aria-hidden="true">↑</span>
                <span className="upload-title">{file ? file.name : 'Drop your file here'}</span>
                <span className="upload-hint">{file ? `${formatBytes(file.size)} · Click to choose another` : 'or click to browse · up to 100 MB'}</span>
              </label>

              {tab === 'Compress' && (
                <div className="options">
                  <label className="field-label" htmlFor="algorithm">Compression algorithm</label>
                  <select id="algorithm" value={algorithm} onChange={(event) => setAlgorithm(event.target.value)}>
                    {algorithms.map((item) => <option key={item.id} value={item.id}>{item.name} — {item.description}</option>)}
                  </select>
                  <label className="checkbox-row">
                    <input type="checkbox" checked={trace} onChange={(event) => setTrace(event.target.checked)} />
                    <span><strong>Capture algorithm trace</strong><small>Record execution steps for analysis</small></span>
                  </label>
                </div>
              )}

              {tab === 'Decompress' && <p className="format-hint">Choose an SFC compressed file (.sfc) to restore its original contents.</p>}

              <button className="button button-primary action-button" disabled={busy || !health?.engineAvailable} onClick={() => {
                if (tab === 'Compress') void runAction('compress')
                else if (tab === 'Analyze') void runAction('analyze')
                else if (tab === 'Benchmark') void runAction('benchmark')
                else void runAction('decompress')
              }}>
                {busy ? <><span className="spinner" /> Working…</> : tab === 'Compress' ? 'Compress file' : tab === 'Analyze' ? 'Analyze file' : tab === 'Benchmark' ? 'Run benchmark' : 'Decompress file'}
                {!busy && <span aria-hidden="true">↗</span>}
              </button>
              {!health?.engineAvailable && <p className="connection-note">The backend or compression engine is not available. Start the backend and retry.</p>}
              {error && <p className="error-message" role="alert">{error}</p>}
            </div>

            <aside className="aside-panel">
              <div className="aside-top">
                <p className="eyebrow">{result ? 'RESULTS' : 'HOW IT WORKS'}</p>
                <h3>{result ? 'Your file, processed.' : 'Simple by design.'}</h3>
                <p>{result ? 'The engine has finished processing your file. Review the details below.' : 'Choose a file and an operation. The local compression engine handles the rest.'}</p>
              </div>
              {result ? (
                <>
                  {downloadUrl &&                   <a className="button button-download" href={`${API_ROOT}${downloadUrl}`}>Download output <span>↓</span></a>}
                  {benchmarkResults.length > 0 && (
                    <div className="benchmark-list">
                      {benchmarkResults.map((item, index) => (
                        <div className="benchmark-item" key={`${String(item.algorithmSlug)}-${index}`}>
                          <span>{String(item.algorithmName ?? item.algorithmSlug ?? `Algorithm ${index + 1}`)}</span>
                          <strong>{typeof item.spaceSavedPercent === 'number' ? `${item.spaceSavedPercent.toFixed(1)}% saved` : formatBytes(typeof item.compressedSize === 'number' ? item.compressedSize : undefined)}</strong>
                        </div>
                      ))}
                      {typeof result.empiricalWinner === 'string' && <p className="winner">Best result: <strong>{result.empiricalWinner}</strong></p>}
                    </div>
                  )}
                  {analysis && (
                    <div className="analysis-summary">
                      <div><span>Entropy</span><strong>{displayValue(analysis.entropy)} bits/byte</strong></div>
                      <div><span>Distinct symbols</span><strong>{displayValue(analysis.distinctSymbols)}</strong></div>
                      <div><span>Suggested algorithm</span><strong>{displayValue(analysis.recommendedAlgorithm)}</strong></div>
                      {typeof analysis.recommendationReason === 'string' && <p>{analysis.recommendationReason}</p>}
                    </div>
                  )}
                  {resultRows.length > 0 && <div className="result-grid">{resultRows.map(([key, value]) => (
                    <div className="result-item" key={key}><span>{labelFor(key)}</span><strong>{key.toLowerCase().includes('size') ? formatBytes(Number(value)) : displayValue(value)}</strong></div>
                  ))}</div>}
                </>
              ) : (
                <div className="steps">
                  <div className="step"><span>01</span><div><strong>Choose a file</strong><p>Upload a file from your device.</p></div></div>
                  <div className="step"><span>02</span><div><strong>Select an operation</strong><p>Compress, analyze, benchmark, or restore.</p></div></div>
                  <div className="step"><span>03</span><div><strong>Get your results</strong><p>Review the metrics and download the output.</p></div></div>
                </div>
              )}
            </aside>
          </>
        )}
      </section>

      <footer><span>BYTEWORKS</span><span>Huffman · LZW · RLE</span><span>Files stay on your machine</span></footer>
    </main>
  )
}

export default App
