import React from 'react'
import { Zap, Usb } from 'lucide-react'
import { cn, parsePayloadName } from '../../utils/helpers'
import { useTranslation } from 'react-i18next'

const PayloadName = ({ path, name, version: repoVersion, className, versionClassName, stacked = false, hideIcon = false, lastUpdate = null, minFw = null, maxFw = null, fwIncompatible = false }) => {
  const { t } = useTranslation();
  let { displayName, version, isDelay, delaySeconds } = parsePayloadName(path);
  const isUsb = path?.startsWith('/mnt/usb');

  if (repoVersion) version = repoVersion;
  // Prefer the repository "name" field when it's a real display name (not just a
  // filename). Payloads without metadata (USB / manual uploads whose stored name
  // is the filename) fall back to the cleaned filename.
  if (name && !isDelay && !/\.(elf|bin)$/i.test(name)) {
    displayName = name;
  }
  if (isDelay) {
    displayName = t("autoload.delay_item", "Delay ({{seconds}}s)", { seconds: delaySeconds || 1 });
  }

  // Compose a compact firmware-range label from min/max FW.
  const fwLabel = (minFw && maxFw) ? `FW ${minFw}–${maxFw}`
    : minFw ? `FW ≥ ${minFw}`
    : maxFw ? `FW ≤ ${maxFw}`
    : null;

  return (
    <div className={cn("flex min-w-0 flex-1 w-full", stacked ? "flex-col items-stretch" : "items-center space-x-3", className)}>
      <div className="flex items-center space-x-2 min-w-0">
        {isDelay && !hideIcon && <Zap className="w-4 h-4 text-ps-blue shrink-0" />}
        {isUsb && !hideIcon && <Usb className="w-5 h-5 text-ps-blue shrink-0 mr-1" />}
        <span className="font-bold truncate shrink leading-tight">{displayName}</span>
      </div>
      {(version || lastUpdate || fwLabel) && (
        <div className={cn("flex items-center flex-wrap gap-y-1", stacked ? "mt-1" : "")}>
          {version && (
            <span className={cn(
              stacked
                ? "text-[11px] font-bold tracking-wider text-ps-blue opacity-90"
                : "text-[10px] px-2 py-0.5 bg-ps-blue/10 text-ps-blue font-bold rounded-md border border-ps-blue/20 shrink-0",
              versionClassName)}>
              {version}
            </span>
          )}
          {version && (lastUpdate || fwLabel) && (
            <span className="text-[11px] text-zinc-600 mx-2">•</span>
          )}
          {fwLabel && (
            <span className={cn(
              "font-bold tracking-wider shrink-0",
              stacked ? "text-[11px]" : "text-[10px] px-2 py-0.5 rounded-md border",
              fwIncompatible
                ? (stacked ? "text-red-400" : "text-red-400 bg-red-500/10 border-red-500/30")
                : (stacked ? "text-amber-400/90" : "text-amber-400 bg-amber-500/10 border-amber-500/30")
            )}>
              {fwLabel}
            </span>
          )}
          {fwLabel && lastUpdate && (
            <span className="text-[11px] text-zinc-600 mx-2">•</span>
          )}
          {lastUpdate && (
            <span className="text-[11px] font-bold tracking-wider text-zinc-500 uppercase">
              {lastUpdate}
            </span>
          )}
        </div>
      )}
    </div>
  );
};

export default PayloadName
