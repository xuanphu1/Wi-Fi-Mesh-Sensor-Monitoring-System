import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { Activity, Bell, Share2, Cpu, Database, Router, Monitor, Briefcase } from 'lucide-react-native';
import { useRouter } from 'expo-router';
import { COLORS, SIZES } from '../../src/constants/theme';
import SettingMenuCard from '../../src/components/SettingMenuCard';

export default function SettingsScreen() {
  const router = useRouter();

  const menuItems = [
    {
      title: 'Sensor Thresholds',
      subtitle: 'Set min/max limits for each sensor field',
      icon: <Activity size={24} color="#38bdf8" />, // Info blue
      iconColor: '#38bdf8',
      route: '/settings/thresholds'
    },
    {
      title: 'Alerts',
      subtitle: 'Configure warning, critical and offline alerts',
      icon: <Bell size={24} color="#fb923c" />, // Orange
      iconColor: '#fb923c',
      route: '/settings/alerts'
    },
    {
      title: 'Nodes',
      subtitle: 'Manage node names, locations and status',
      icon: <Share2 size={24} color={COLORS.success} />, // Green
      iconColor: COLORS.success,
      route: '/settings/nodes'
    },
    {
      title: 'Ports & Sensors',
      subtitle: 'Map sensor types to node ports and configure fields',
      icon: <Cpu size={24} color="#a875ff" />, // Purple
      iconColor: '#a875ff',
      route: '/settings/ports'
    },
    {
      title: 'Data & History',
      subtitle: 'Refresh rate, storage time and export options',
      icon: <Database size={24} color="#2dd4bf" />, // Teal
      iconColor: '#2dd4bf',
      route: '/settings/data-history'
    },
    {
      title: 'Gateway',
      subtitle: 'Root, UART, WebSocket and server settings',
      icon: <Router size={24} color="#3b82f6" />, // Blue
      iconColor: '#3b82f6',
      route: '/settings/gateway'
    },
    {
      title: 'Display',
      subtitle: 'Theme, compact mode and visible fields',
      icon: <Monitor size={24} color="#e81cff" />, // Magenta
      iconColor: '#e81cff',
      route: '/settings/display'
    },
    {
      title: 'Backup & Reset',
      subtitle: 'Export config, import config and reset system',
      icon: <Briefcase size={24} color="#eab308" />, // Yellow
      iconColor: '#eab308',
      route: '/settings/backup'
    }
  ];

  return (
    <SafeAreaView style={styles.container} edges={['top']}>
      {/* Header */}
      <View style={styles.header}>
        <Text style={styles.headerTitle}>Settings</Text>
        <Text style={styles.headerSubtitle}>Configure your system and preferences</Text>
      </View>

      <ScrollView contentContainerStyle={styles.scrollContent} showsVerticalScrollIndicator={false}>
        {menuItems.map((item, idx) => (
          <SettingMenuCard
            key={idx}
            title={item.title}
            subtitle={item.subtitle}
            icon={item.icon}
            iconColor={item.iconColor}
            onPress={() => {
              // Attempt to navigate. Ensure placeholders exist!
              try {
                router.push(item.route as any);
              } catch (error) {
                console.log("Route not implemented yet:", item.route);
              }
            }}
          />
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
  },
  header: {
    paddingHorizontal: SIZES.large,
    paddingVertical: SIZES.large,
    paddingBottom: SIZES.medium,
  },
  headerTitle: {
    fontSize: SIZES.extraLarge,
    fontWeight: 'bold',
    color: COLORS.textMain,
    marginBottom: 4,
  },
  headerSubtitle: {
    fontSize: SIZES.small,
    color: COLORS.secondary,
  },
  scrollContent: {
    padding: SIZES.large,
    paddingTop: SIZES.small,
    paddingBottom: 40,
  }
});
