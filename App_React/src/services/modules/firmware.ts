import apiClient from '@/services/api';
import type { FirmwareMeta } from '@/types/mesh';

export async function listFirmware() {
  const { data } = await apiClient.get<{ firmware: FirmwareMeta[] }>('/api/v1/firmware');
  return data.firmware;
}
