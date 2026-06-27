import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { Stack } from 'expo-router';
import { StatusBar } from 'expo-status-bar';
import { useEffect, useMemo } from 'react';

import { WebSocketProvider } from '@/components/WebSocketProvider';
import GlobalToast from '@/components/GlobalToast';
import { loadSavedEndpoint } from '@/services/api';

function RuntimeBootstrap() {
  useEffect(() => {
    void loadSavedEndpoint();
  }, []);
  return null;
}

export default function RootLayout() {
  const queryClient = useMemo(() => new QueryClient(), []);

  return (
    <QueryClientProvider client={queryClient}>
      <WebSocketProvider>
        <RuntimeBootstrap />
        <StatusBar style="light" />
        <Stack screenOptions={{ headerShown: false }}>
          <Stack.Screen name="(tabs)" />
          <Stack.Screen name="+not-found" />
        </Stack>
        <GlobalToast />
      </WebSocketProvider>
    </QueryClientProvider>
  );
}
