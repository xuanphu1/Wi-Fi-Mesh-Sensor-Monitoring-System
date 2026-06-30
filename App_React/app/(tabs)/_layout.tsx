import { Tabs } from 'expo-router';
import { Home, Cpu, Activity, Settings, History, HardDrive, Server, Info } from 'lucide-react-native';
import { COLORS } from '../../src/constants/theme';

export default function TabsLayout() {
  return (
    <Tabs
      screenOptions={{
        headerShown: false,
        headerStyle: { backgroundColor: COLORS.background },
        headerTintColor: COLORS.textMain,
        tabBarStyle: { backgroundColor: COLORS.surface, borderTopColor: COLORS.border },
        tabBarActiveTintColor: COLORS.primary,
        tabBarInactiveTintColor: COLORS.textMuted,
      }}
    >
      <Tabs.Screen name="index" options={{ title: 'Dashboard', tabBarIcon: ({ color }) => <Home size={22} color={color} /> }} />
      <Tabs.Screen name="nodes" options={{ title: 'Nodes', tabBarIcon: ({ color }) => <Cpu size={22} color={color} /> }} />
      <Tabs.Screen name="sensors" options={{ title: 'Sensors', tabBarIcon: ({ color }) => <Activity size={22} color={color} /> }} />
      <Tabs.Screen name="gateway" options={{ title: 'Gateway', tabBarIcon: ({ color }) => <Server size={22} color={color} /> }} />
      <Tabs.Screen name="server" options={{ title: 'Server', tabBarIcon: ({ color }) => <HardDrive size={22} color={color} /> }} />
      <Tabs.Screen name="settings" options={{ title: 'Settings', tabBarIcon: ({ color }) => <Settings size={22} color={color} /> }} />
      <Tabs.Screen name="history" options={{ title: 'History', tabBarIcon: ({ color }) => <History size={22} color={color} /> }} />
      <Tabs.Screen name="about" options={{ title: 'About', tabBarIcon: ({ color }) => <Info size={22} color={color} /> }} />
      <Tabs.Screen name="firmware" options={{ href: null }} />
    </Tabs>
  );
}
