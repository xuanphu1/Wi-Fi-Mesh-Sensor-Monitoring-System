import apiClient from '@/services/api';
import type { HistoryIpRow, HistorySeriesItem } from '@/types/mesh';

export async function getHealth() {
  const { data } = await apiClient.get<{ ok: boolean; service: string }>('/api/v1/health');
  return data;
}

export async function getHistoryMeta(limit = 200) {
  const { data } = await apiClient.get<{ ips: HistoryIpRow[] }>('/api/v1/history/meta', {
    params: { limit },
  });
  return data.ips;
}

export async function getHistorySensors(ip: string) {
  const { data } = await apiClient.get<{ ip: string; sensors: string[] }>('/api/v1/history/sensors', {
    params: { ip },
  });
  return data.sensors;
}

export async function getHistorySeries(params: {
  ip?: string;
  sensor?: string;
  field?: string;
  limit?: number;
}) {
  const { data } = await apiClient.get<{ items: HistorySeriesItem[] }>('/api/v1/history/series', {
    params,
  });
  return data.items;
}
