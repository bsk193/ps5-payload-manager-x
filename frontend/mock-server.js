import express from 'express';
import cors from 'cors';

const app = express();
app.use(cors());
app.use(express.json());

// Log all requests to terminal
app.use((req, res, next) => {
  console.log(`[Mock] ${req.method} ${req.url}`);
  next();
});

const PORT = 8081;

let logs = [
  "[PLDMGR] Mock Server initialized",
  "[PLDMGR] System ready",
  "[PLDMGR] Found 3 payloads on storage"
];

const remoteRepository = [
  {
    name: "ftpsrv",
    filename: "ftpsrv_v0.19.elf",
    url: "https://itsplk.github.io/ps5_payloads/payloads/ftpsrv_v0.19.elf",
    description: "A simple FTP server that accepts connections on port 2121",
    version: "v0.19",
    min_fw: "1.00",
    max_fw: "10.20",
    checksum: "e6c1babbfd5e1b766d12b659853b514b9faedf6333cbe8cb514b1a3e79b7ce39"
  },
  {
    name: "KStuff Lite",
    filename: "kstuff-lite_v1.09.elf",
    url: "https://bsk193.github.io/ps5-payloads-mirror/payloads/kstuff-lite_v1.09.elf",
    description: "Lite version of kstuff (latest)",
    version: "v1.09",
    min_fw: "1.00",
    max_fw: "10.20",
    checksum: "54df47a48d9c5ee4338ef70ba66093908a4f2845e53468bdd7c080b65d7488c1"
  },
  {
    name: "KStuff Lite",
    filename: "kstuff-lite_v1.06.elf",
    url: "https://bsk193.github.io/ps5-payloads-mirror/payloads/kstuff-lite_v1.06.elf",
    description: "Lite version of kstuff (older FW)",
    version: "v1.06",
    min_fw: "1.00",
    max_fw: "7.61",
    checksum: "aaaa47a48d9c5ee4338ef70ba66093908a4f2845e53468bdd7c080b65d7488c2"
  }
];

const communityRepository = [
  {
    name: "PS5 Hen Test",
    filename: "ps5_hen_test_v1.0.elf",
    url: "https://example.com/payloads/ps5_hen_test_v1.0.elf",
    description: "Community hen test payload — no checksum",
    version: "v1.0"
  },
  {
    name: "Debug Tool",
    filename: "debug_tool_v2.1.elf",
    url: "https://example.com/payloads/debug_tool_v2.1.elf",
    description: "Debug utility from community repo",
    version: "v2.1"
  }
];

let lastRepositoryUpdate = Math.floor(Date.now() / 1000);

let mockSources = [
  { id: 'default', name: 'Official Repository', url: 'https://bsk193.github.io/ps5-payloads-mirror/payloads.json', removable: false },
  { id: 'source_community', name: 'Community Payloads', url: 'https://example.com/community-payloads.json', removable: true }
];

// Profiles (autoload sequences). `list` is a comma-separated string on the wire.
// Default: two profiles, none enabled -> boots into the startup picker so the
// new feature is visible immediately. Enable one to test the auto-run path.
let mockProfiles = [
  { id: 'gaming', name: 'Gaming', enabled: false, list: 'goldhen_v2.4b17.elf,!1000,etaHEN_1.8.elf' },
  { id: 'development', name: 'Development', enabled: false, list: 'kstuff.elf' }
];

let autoloadStatus = {
  remaining: -1,
  remaining_ms: -1000,
  total: 0,
  done: 0,
  current: "IDLE",
  list: "",
  delay: 5,
  picker: false
};

let seqEntries = [];       // payloads of the currently running sequence (no delays)
let simulationTicks = 0;

function resolveActive() {
  return mockProfiles.find(p => p.enabled === true || p.enabled === 'true') || null;
}

function startSequence(listStr, withCountdown) {
  const entries = String(listStr || '').split(',').filter(Boolean);
  seqEntries = entries.filter(e => !e.startsWith('!'));
  autoloadStatus.list = seqEntries.join(',');
  autoloadStatus.total = seqEntries.length;
  autoloadStatus.done = 0;
  autoloadStatus.picker = false;
  autoloadStatus.delay = 5;
  simulationTicks = 0;
  if (withCountdown) {
    autoloadStatus.remaining = 5;
    autoloadStatus.remaining_ms = 5000;
    autoloadStatus.current = '';
  } else {
    autoloadStatus.remaining = 0;
    autoloadStatus.remaining_ms = 0;
    autoloadStatus.current = seqEntries[0] || 'DONE';
  }
}

// Boot decision (mirrors the backend autoload worker).
(function boot() {
  const active = resolveActive();
  if (active) {
    logs.push(`[PLDMGR] Autoload: enabled profile '${active.name}'`);
    startSequence(active.list, true);
  } else if (mockProfiles.length > 0) {
    logs.push(`[PLDMGR] Autoload: no enabled profile - showing startup picker`);
    autoloadStatus.picker = true;
    autoloadStatus.remaining = -1;
    autoloadStatus.remaining_ms = -1000;
  } else {
    logs.push(`[PLDMGR] Autoload: no profiles - booting to dashboard`);
  }
})();

setInterval(() => {
  if (autoloadStatus.remaining > 0) {
    autoloadStatus.remaining--;
    autoloadStatus.remaining_ms = autoloadStatus.remaining * 1000;
    if (autoloadStatus.remaining === 0) {
      autoloadStatus.current = seqEntries[0] || 'DONE';
      logs.push(`[PLDMGR] Autoload sequence started: ${autoloadStatus.current}`);
    }
  } else if (autoloadStatus.remaining === 0 && autoloadStatus.current !== "DONE" && autoloadStatus.current !== "IDLE") {
    simulationTicks++;
    if (simulationTicks >= 3) { // 3 seconds per payload simulation
      simulationTicks = 0;
      autoloadStatus.done++;
      if (autoloadStatus.done < autoloadStatus.total) {
        autoloadStatus.current = seqEntries[autoloadStatus.done];
        logs.push(`[PLDMGR] Autoloading: ${autoloadStatus.current}`);
      } else {
        autoloadStatus.current = "DONE";
        logs.push(`[PLDMGR] Autoload sequence complete`);
      }
    }
  }
}, 1000);

// --- API Routes ---

app.get('/getip', (req, res) => {
  res.send('127.0.0.1');
});

app.get('/version', (req, res) => {
  res.send('1.0.2-mock');
});

app.get('/system_info', (req, res) => {
  // Simulate a console on FW 10.20 -> payloads with max_fw < 10.20 are incompatible.
  res.json({ fw: '10.20' });
});

app.get('/list_payloads', (req, res) => {
  res.json({
    payloads: [
      "/data/pldmgr/goldhen_v2.4b17.elf",
      "/data/pldmgr/etaHEN_1.8.elf",
      "/data/pldmgr/kstuff.elf",
      "/mnt/usb0/pldmgr/linux_loader.elf"
    ],
    meta: {
      // payloads with no entry here = local/direct upload (no badge shown)
      "goldhen_v2.4b17.elf": { version: "v2.4b17", min_fw: "1.00", max_fw: "7.61" },
      "etaHEN_1.8.elf": { version: "1.8", min_fw: "1.00", max_fw: "10.20" },
      "kstuff.elf": {
        source_name: "Community Payloads",
        install_source: "repository",
        install_source_detail: "https://example.com/community-payloads.json",
        version: "v1.03",
        min_fw: "2.50",
        max_fw: "10.01"
      }
    }
  });
});

app.get('/autoload_status', (req, res) => {
  res.json(autoloadStatus);
});

app.get('/get_config', (req, res) => {
  res.json({
    AUTOLOAD_ENABLED: true,
    AUTOLOAD_LIST: "goldhen_v2.4b17.elf,etaHEN_1.8.elf",
    LAST_REPOSITORY_UPDATE: lastRepositoryUpdate,
    AUTO_INSTALL_APP: true,
    MULTI_SOURCES_ENABLED: true
  });
});

app.get('/repository_payloads', (req, res) => {
  // Return source-grouped format (multi-source mode always on in mock)
  res.json({
    sources: [
      {
        id: 'default',
        name: 'Official Repository',
        last_update: lastRepositoryUpdate,
        error: false,
        payloads: remoteRepository.map(p => ({ ...p, source_id: 'default', source_name: 'Official Repository' }))
      },
      {
        id: 'source_community',
        name: 'Community Payloads',
        last_update: lastRepositoryUpdate,
        error: false,
        payloads: communityRepository.map(p => ({ ...p, source_id: 'source_community', source_name: 'Community Payloads' }))
      }
    ]
  });
});

app.get('/repository_refresh', (req, res) => {
  lastRepositoryUpdate = Math.floor(Date.now() / 1000);
  logs.push(`[PLDMGR] Repository manually refreshed`);
  res.json({
    sources: [
      {
        id: 'default',
        name: 'Official Repository',
        last_update: lastRepositoryUpdate,
        error: false,
        payloads: remoteRepository.map(p => ({ ...p, source_id: 'default', source_name: 'Official Repository' }))
      },
      {
        id: 'source_community',
        name: 'Community Payloads',
        last_update: lastRepositoryUpdate,
        error: false,
        payloads: communityRepository.map(p => ({ ...p, source_id: 'source_community', source_name: 'Community Payloads' }))
      }
    ]
  });
});

app.get('/sources_list', (req, res) => {
  res.json({ sources: mockSources });
});

app.post('/sources_set', (req, res) => {
  const { sources } = req.body;
  if (Array.isArray(sources)) {
    mockSources = sources;
    logs.push(`[PLDMGR] Sources updated: ${sources.length} sources`);
  }
  res.send('OK');
});

app.get('/sources_add', (req, res) => {
  const { url } = req.query;
  if (!url) return res.status(400).json({ ok: false, message: 'Missing url' });
  // Mock: always succeed with a fake source name derived from the URL
  const fakeName = `Community Source (${new URL(url).hostname})`;
  const newSource = { id: `source_${Date.now()}`, name: fakeName, url, removable: true };
  mockSources.push(newSource);
  logs.push(`[PLDMGR] Source added: ${fakeName}`);
  res.json({ ok: true, name: fakeName });
});

app.get('/sources_remove', (req, res) => {
  const idx = parseInt(req.query.index);
  if (isNaN(idx) || idx <= 0 || idx >= mockSources.length) {
    return res.status(400).json({ ok: false, message: 'Invalid index or cannot remove default source' });
  }
  const removed = mockSources.splice(idx, 1);
  logs.push(`[PLDMGR] Source removed: ${removed[0]?.name}`);
  res.json({ ok: true, message: 'OK' });
});

app.get('/repository_install', (req, res) => {
  const { filename } = req.query;
  if (!filename) {
    return res.status(400).json({ ok: false, message: 'Missing filename' });
  }
  logs.push(`[PLDMGR] Repository install requested: ${filename}`);
  res.json({ ok: true, message: `Installed ${filename}` });
});

app.post('/set_config', (req, res) => {
  console.log('Received Config:', req.body);
  logs.push(`[PLDMGR] Config updated: ${JSON.stringify(req.body)}`);
  res.send('OK');
});

// --- Profiles ---
app.get('/profiles_get', (req, res) => {
  res.json({ profiles: mockProfiles });
});

app.post('/profiles_set', (req, res) => {
  if (Array.isArray(req.body.profiles)) {
    // Enforce at-most-one-enabled, like the backend.
    let seen = false;
    mockProfiles = req.body.profiles.map(p => {
      const enabled = (p.enabled === true || p.enabled === 'true') && !seen;
      if (enabled) seen = true;
      return { id: p.id, name: p.name, enabled, list: p.list || '' };
    });
    logs.push(`[PLDMGR] Profiles updated: ${mockProfiles.length} profiles`);
  }
  res.send('OK');
});

// --- Startup payloads (always-run list) ---
let mockStartup = [];
app.get('/startup_get', (req, res) => res.json({ startup: mockStartup }));
app.get('/startup_toggle', (req, res) => {
  const f = req.query.filename;
  const en = req.query.enabled === '1';
  if (f) {
    mockStartup = en ? Array.from(new Set([...mockStartup, f])) : mockStartup.filter(x => x !== f);
    logs.push(`[PLDMGR] Startup ${en ? 'enabled' : 'disabled'}: ${f}`);
  }
  res.json({ ok: !!f });
});

app.get('/profile_run', (req, res) => {
  const id = req.query.id;
  const p = mockProfiles.find(x => x.id === id);
  if (!p) return res.status(400).json({ ok: false });
  logs.push(`[PLDMGR] Manual run: ${p.name}`);
  startSequence(p.list, false); // run immediately, no countdown
  res.json({ ok: true });
});

app.get(/^\/loadpayload:(.*)/, (req, res) => {
  const path = req.params[0];
  logs.push(`[PLDMGR] Executing payload: ${path}`);
  res.send('OK');
});

app.get('/manage\\:delete', (req, res) => {
  const filename = req.query.filename;
  logs.push(`[PLDMGR] Deleted payload: ${filename}`);
  res.send('OK');
});

app.get('/manage\\:check', (req, res) => {
  const filename = (req.query.filename || '').toLowerCase();
  // Simulate: a different version of kstuff-lite is already installed.
  const folderExists = filename.includes('kstuff-lite');
  res.json({ status: 'ok', folder_exists: folderExists, file_exists: false, folder_name: 'kstuff-lite' });
});

app.post('/manage\\:upload', (req, res) => {
  const filename = req.query.filename;
  const keep = req.query.keep === '1';
  logs.push(`[PLDMGR] Uploaded payload: ${filename}${keep ? ' (keep both)' : ''}`);
  res.send('OK');
});

app.get('/abort', (req, res) => {
  logs.push(`[PLDMGR] Autoload sequence aborted by user`);
  autoloadStatus.remaining = -1;
  autoloadStatus.remaining_ms = -1000;
  autoloadStatus.current = "IDLE";
  autoloadStatus.picker = false;
  res.send('OK');
});

app.get('/autoload_clear', (req, res) => {
  logs.push(`[PLDMGR] Autoload status cleared`);
  autoloadStatus.done = 0;
  autoloadStatus.current = "IDLE";
  autoloadStatus.remaining = -1;
  autoloadStatus.remaining_ms = -1000;
  autoloadStatus.picker = false;
  res.send('OK');
});

app.get('/shutdown', (req, res) => {
  logs.push(`[PLDMGR] System shutdown requested`);
  res.send('Shutting down...');
  process.exit(0);
});

// SSE for logs
app.get('/events', (req, res) => {
  res.setHeader('Content-Type', 'text/event-stream');
  res.setHeader('Cache-Control', 'no-cache');
  res.setHeader('Connection', 'keep-alive');

  const sendLog = (data) => {
    res.write(`data: ${data}\n\n`);
  };

  // Send history
  logs.forEach(sendLog);

  // Poll for new logs
  let lastCount = logs.length;
  const interval = setInterval(() => {
    if (logs.length > lastCount) {
      for (let i = lastCount; i < logs.length; i++) {
        sendLog(logs[i]);
      }
      lastCount = logs.length;
    }
  }, 500);

  req.on('close', () => {
    clearInterval(interval);
  });
});

app.listen(PORT, '127.0.0.1', () => {
  console.log(`\x1b[36m%s\x1b[0m`, `--- Payload Manager Mock Backend ---`);
  console.log(`Running at http://localhost:${PORT}`);
  console.log(`Proxy your Vite requests to this port to test the frontend locally.`);
});
