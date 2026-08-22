import { clsx } from 'clsx'
import { twMerge } from 'tailwind-merge'

export const isPS5 = /PlayStation/i.test(navigator.userAgent);
export const isIOS = /iPad|iPhone|iPod/.test(navigator.userAgent) || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);

export function cn(...inputs) {
  return twMerge(clsx(inputs))
}

export const parsePayloadName = (path) => {
  if (!path) return { displayName: '', version: null };
  if (path.startsWith('!')) {
    const ms = parseInt(path.substring(1));
    return { displayName: `Delay (${ms / 1000}s)`, version: null, isDelay: true, delaySeconds: ms / 1000 };
  }

  let name = path.split('/').pop().replace(/\.(elf|bin)$/i, '');

  // Try to find version pattern like _v1.0 or -v1.0 or _1.0
  const versionMatch = name.match(/[_-](v?\d+[\d.a-z-]+)/i);
  let version = null;

  if (versionMatch) {
    version = versionMatch[1];
    name = name.replace(versionMatch[0], '');
  }

  return {
    displayName: name.replace(/_/g, ' ').replace(/-/g, ' '),
    version: version,
    isDelay: false
  };
};

// Version-stripped identity of a payload, mirroring the C
// pldmgr_utils_get_payload_folder_name(): strip extension, a _v/-v (or _/- + digit)
// version marker, and any -ps4/-ps5 suffix. Used to prevent launching/adding two
// versions of the same payload.
export const payloadBaseName = (filename) => {
  if (!filename) return '';
  let clean = filename.split('/').pop();
  const dot = clean.lastIndexOf('.');
  if (dot > 0) clean = clean.slice(0, dot);
  let idx = clean.indexOf('_v');
  if (idx < 0) idx = clean.indexOf('-v');
  if (idx >= 0) {
    clean = clean.slice(0, idx);
  } else {
    const m = clean.match(/[_-]\d/);
    if (m) clean = clean.slice(0, m.index);
  }
  clean = clean.replace(/[_-]ps[45].*$/i, '');
  return clean.toLowerCase();
};

// Compare two version strings numerically, part by part, so "1.10" > "1.6.7"
// (parseFloat would wrongly give 1.1 < 1.6). Leading "v" is ignored; parts split
// on . - _ and non-numeric suffixes (e.g. "b17", "-dr-test1") are dropped.
// Returns >0 if a is newer, <0 if older, 0 if equal/unknown.
export const compareVersions = (a, b) => {
  const parts = (v) => String(v || '').replace(/^v/i, '').split(/[.\-_]/)
    .map(s => parseInt(s, 10)).filter(n => !isNaN(n));
  const pa = parts(a), pb = parts(b);
  const len = Math.max(pa.length, pb.length);
  for (let i = 0; i < len; i++) {
    const x = pa[i] || 0, y = pb[i] || 0;
    if (x !== y) return x - y;
  }
  return 0;
};

// Parse a firmware string like "7.61" or "10.20" to a comparable number.
export const parseFw = (s) => {
  if (!s) return null;
  const n = parseFloat(String(s).replace(/[^0-9.]/g, ''));
  return isNaN(n) ? null : n;
};

// True if the console FW falls within [minFw, maxFw]. When the console FW is
// unknown (null/empty), returns true so nothing is grayed out (display-only).
export const fwCompatible = (minFw, maxFw, consoleFw) => {
  const c = parseFw(consoleFw);
  if (c === null) return true;
  const mn = parseFw(minFw), mx = parseFw(maxFw);
  if (mn !== null && c < mn) return false;
  if (mx !== null && c > mx) return false;
  return true;
};

export const isSystemPayload = (filename) => {
  if (!filename) return false;
  const name = filename.split('/').pop().toLowerCase();
  return name.startsWith('pldmgr') || name.includes('payload-manager');
};
