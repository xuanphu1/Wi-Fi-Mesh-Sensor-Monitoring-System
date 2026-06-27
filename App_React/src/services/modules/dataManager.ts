import apiClient from '@/services/api';
import type { HistorySeriesItem } from '@/types/mesh';

export async function listDataRows(params: {
  ip?: string;
  sensorName?: string;
  field?: string;
  limit?: number;
}) {
  const { data } = await apiClient.get<{ items: HistorySeriesItem[]; count: number }>('/api/v1/data-manager', {
    params,
  });
  return data;
}
