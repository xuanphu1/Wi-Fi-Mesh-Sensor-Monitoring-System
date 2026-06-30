import { create } from 'zustand';

import { GATEWAY_OFFLINE_MS, NODE_OFFLINE_MS } from '@/constants/mesh';
import type { GatewayStatus, MeshNode, ParsedMeshPacket, ServerNetwork, ServerMetrics } from '@/types/mesh';
import { parseDeviceRtcMs } from '@/utils/format';

type MeshStore = {
  apiBaseUrl: string;
  wsUrl: string;
  wsStatus: 'Disconnected' | 'Connecting' | 'Connected';
  serverNetwork: ServerNetwork | null;
  gateway: GatewayStatus | null;
  gatewaySeenAt: number | null;
  gatewaySeries: Array<{ t: number; cpu: number; ram: number; rssi: number | null }>;
  throughputSeries: Array<{ value: number }>;
  nodes: Record<string, MeshNode>;
  eventLog: Array<{ t: number; level: 'info' | 'warn'; message: string }>;
  serverMetrics: ServerMetrics | null;
  serverMetricsSeries: Array<{ t: number; cpu: number; ram: number }>;
  setEndpoint: (apiBaseUrl: string, wsUrl: string) => void;
  setWsStatus: (status: MeshStore['wsStatus']) => void;
  applyWelcome: (payload: unknown) => void;
  ingestGateway: (gateway: GatewayStatus) => void;
  ingestServerMetrics: (metrics: ServerMetrics) => void;
  ingestPacket: (packet: ParsedMeshPacket, payloadBytes?: number) => void;
  onlineNodes: () => MeshNode[];
  allNodes: () => MeshNode[];
  isGatewayOnline: () => boolean;
  checkTimeouts: () => void;
};

function defaultApiBaseUrl() {
  return 'http://127.0.0.1:8080';
}

function defaultWsUrl() {
  return 'ws://127.0.0.1:8080';
}

function nodeStatus(lastSeen: number | null) {
  if (!lastSeen) return 'Offline';
  const age = Date.now() - lastSeen;
  return age >= 0 && age <= NODE_OFFLINE_MS ? 'Online' : 'Offline';
}

export const useMeshStore = create<MeshStore>((set, get) => ({
  apiBaseUrl: defaultApiBaseUrl(),
  wsUrl: defaultWsUrl(),
  wsStatus: 'Disconnected',
  serverNetwork: null,
  gateway: null,
  gatewaySeenAt: null,
  gatewaySeries: [],
  throughputSeries: Array.from({ length: 30 }, () => ({ value: 0 })),
  nodes: {},
  eventLog: [],
  serverMetrics: null,
  serverMetricsSeries: [],

  setEndpoint: (apiBaseUrl, wsUrl) => set({ apiBaseUrl, wsUrl }),
  setWsStatus: (status) => set({ wsStatus: status }),

  applyWelcome: (payload) => {
    if (!payload || typeof payload !== 'object') return;
    const data = payload as Record<string, unknown>;
    const snapshot = Array.isArray(data.nodeSnapshot) ? data.nodeSnapshot : [];
    const nextNodes = { ...get().nodes };

    snapshot.forEach((row) => {
      if (!row || typeof row !== 'object') return;
      const n = row as Record<string, unknown>;
      const ip = String(n.ip || n.id || '').trim();
      if (!ip) return;
      const lastSeenMs = n.lastSeen ? new Date(String(n.lastSeen)).getTime() : null;
      const previous = nextNodes[ip];
      nextNodes[ip] = {
        ip,
        name: previous?.name || 'Mesh Node',
        status: nodeStatus(lastSeenMs),
        lastSeen: Number.isFinite(lastSeenMs) ? lastSeenMs : previous?.lastSeen || null,
        schemaVersion: Number(n.schemaVersion || previous?.schemaVersion || 0),
        firmwareVersion: previous?.firmwareVersion || '0.0.0',
        meshLevel: Number(n.meshLevel || previous?.meshLevel || 0),
        validRtc: previous?.validRtc || false,
        latencyMs: previous?.latencyMs || null,
        runtimeErrors: previous?.runtimeErrors || [],
        sensorEntries: previous?.sensorEntries || [],
        portCount: previous?.portCount || 0,
      };
    });

    set({
      serverNetwork: (data.serverNet as ServerNetwork) || null,
      nodes: nextNodes,
    });
  },

  ingestGateway: (gateway) => {
    const t = Date.now();
    const event = `Gateway CPU ${Math.round(gateway.cpu_load_percent)}%, RAM ${Math.round(gateway.ram_used_percent)}%, RSSI ${
      gateway.wifi_rssi === 0 ? 'N/A' : `${gateway.wifi_rssi} dBm`
    }`;
    set((state) => ({
      gateway,
      gatewaySeenAt: t,
      gatewaySeries: [
        ...state.gatewaySeries,
        {
          t,
          cpu: gateway.cpu_load_percent,
          ram: gateway.ram_used_percent,
          rssi: gateway.wifi_rssi === 0 ? null : gateway.wifi_rssi,
        },
      ].slice(-40),
      eventLog: [{ t, level: 'info' as const, message: event }, ...state.eventLog].slice(0, 20),
    }));
  },

  ingestServerMetrics: (metrics) => {
    const t = Date.now();
    set((state) => ({
      serverMetrics: metrics,
      serverMetricsSeries: [
        ...state.serverMetricsSeries,
        { t, cpu: metrics.cpuLoadPercent, ram: metrics.ramUsedPercent }
      ].slice(-30),
    }));
  },

  ingestPacket: (packet, payloadBytes = 0) => {
    const now = Date.now();
    const rtcMs = parseDeviceRtcMs(packet.rtcIso);
    const latencyMs = rtcMs != null ? Math.max(0, now - rtcMs) : null;
    const incomingSensorEntries = packet.ports.flatMap((port) =>
      port.readings.map((reading) => ({
        ...reading,
        sensorName: port.sensorName,
        port: port.wirePort,
      })),
    );

    set((state) => {
      const prevNode = state.nodes[packet.mac];
      const mergedSensorEntries = [...(prevNode?.sensorEntries || [])];
      
      incomingSensorEntries.forEach((newEntry) => {
        const existingIdx = mergedSensorEntries.findIndex(
          (e) => e.port === newEntry.port && e.key === newEntry.key
        );
        if (existingIdx >= 0) {
          mergedSensorEntries[existingIdx] = newEntry;
        } else {
          mergedSensorEntries.push(newEntry);
        }
      });

      return {
        nodes: {
          ...state.nodes,
          [packet.mac]: {
            ip: packet.mac,
            name: prevNode?.name || 'Mesh Node',
            status: 'Online',
            lastSeen: now,
            schemaVersion: packet.schemaVersion,
            firmwareVersion: packet.firmwareVersion,
            meshLevel: packet.meshLevel,
            validRtc: rtcMs != null,
            latencyMs,
            runtimeErrors: packet.runtimeErrors,
            sensorEntries: mergedSensorEntries,
            portCount: Math.max(prevNode?.portCount || 0, packet.ports.length),
          },
        },
        throughputSeries: [...state.throughputSeries, { value: payloadBytes }].slice(-30),
      eventLog: [
        {
          t: now,
          level: packet.runtimeErrors.length ? ('warn' as const) : ('info' as const),
          message: `${packet.mac} ${incomingSensorEntries.length} readings, ${payloadBytes} bytes`,
        },
        ...state.eventLog,
      ].slice(0, 20),
      };
    });
  },

  allNodes: () => Object.values(get().nodes).map((node) => ({ ...node, status: nodeStatus(node.lastSeen) })),
  onlineNodes: () => get().allNodes().filter((node) => node.status === 'Online'),
  isGatewayOnline: () => {
    const seenAt = get().gatewaySeenAt;
    return Boolean(seenAt && Date.now() - seenAt <= GATEWAY_OFFLINE_MS);
  },
  checkTimeouts: () => {
    const now = Date.now();
    let hasChanges = false;
    const nextNodes = { ...get().nodes };
    
    Object.values(nextNodes).forEach(node => {
      if (node.status === 'Online') {
        const age = now - (node.lastSeen || 0);
        if (age > NODE_OFFLINE_MS) {
          nextNodes[node.ip] = { ...node, status: 'Offline' };
          hasChanges = true;
        }
      }
    });

    if (hasChanges) {
      set({ nodes: nextNodes });
    }
  }
}));
