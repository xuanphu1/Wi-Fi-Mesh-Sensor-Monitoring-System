import axios from 'axios';

import { getStoredItem, setStoredItem } from '@/services/storage';
import { useMeshStore } from '@/store/useMeshStore';

import Constants from 'expo-constants';
import { Platform } from 'react-native';

export const API_BASE_URL_KEY = 'mesh_api_base_url';
export const WS_URL_KEY = 'mesh_ws_url';

const debuggerHost = Constants.expoConfig?.hostUri;
let defaultHost = '127.0.0.1';
if (debuggerHost) {
  defaultHost = debuggerHost.split(':')[0];
} else if (Platform.OS === 'android') {
  defaultHost = '10.0.2.2';
}

let currentApiBaseUrl = `http://${defaultHost}:8080`;

export const setGlobalApiBaseUrl = (url: string) => {
  currentApiBaseUrl = url;
};

const apiClient = axios.create({
  timeout: 10000,
  headers: { 'Content-Type': 'application/json' },
});

apiClient.interceptors.request.use(async (config) => {
  const baseURL = useMeshStore.getState().apiBaseUrl || currentApiBaseUrl;
  config.baseURL = baseURL;

  const token = await getStoredItem('user_token');
  if (token && config.headers) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

export async function loadSavedEndpoint() {
  const apiBaseUrl = (await getStoredItem(API_BASE_URL_KEY)) || `http://${defaultHost}:8080`;
  const wsUrl = (await getStoredItem(WS_URL_KEY)) || `ws://${defaultHost}:8765`;
  setGlobalApiBaseUrl(apiBaseUrl);
  useMeshStore.getState().setEndpoint(apiBaseUrl, wsUrl);
  return { apiBaseUrl, wsUrl };
}

export async function saveEndpoint(apiBaseUrl: string, wsUrl: string) {
  const cleanApi = apiBaseUrl.trim().replace(/\/+$/, '');
  const cleanWs = wsUrl.trim();
  await setStoredItem(API_BASE_URL_KEY, cleanApi);
  await setStoredItem(WS_URL_KEY, cleanWs);
  useMeshStore.getState().setEndpoint(cleanApi, cleanWs);
}

export default apiClient;
