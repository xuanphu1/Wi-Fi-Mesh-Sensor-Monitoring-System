import apiClient from '@/services/api';
import type { HistoryMacRow, HistorySeriesItem } from '@/types/mesh';

export async function getHealth() {
  const { data } = await apiClient.get<{ ok: boolean; service: string }>('/api/v1/health');
  return data;
}

export async function getHistoryMeta(limit = 200) {
  const { data } = await apiClient.get<{ macs: HistoryMacRow[] }>('/api/history/meta', {
    params: { limit },
  });
  return data.macs;
}

export async function getHistorySeries(params: {
  mac?: string;
  sensorName?: string;
  field?: string;
  from?: string;
  to?: string;
  limit?: number;
}) {
  const { data } = await apiClient.get<{ items: HistorySeriesItem[] }>('/api/history/series', {
    params,
  });
  return data.items;
}
