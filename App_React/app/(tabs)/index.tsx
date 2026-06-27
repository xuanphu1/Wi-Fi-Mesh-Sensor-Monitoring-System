import { View, Text, StyleSheet, ScrollView, TouchableOpacity, Alert } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { Wifi, Menu, Bell, Server, Activity, Clock, AlertTriangle, History, Settings, ShieldCheck, ShieldAlert } from 'lucide-react-native';
import { LinearGradient } from 'expo-linear-gradient';
import { useRouter } from 'expo-router';
import { useState, useEffect } from 'react';
import { COLORS, SIZES } from '../../src/constants/theme';
import MetricTile from '../../src/components/MetricTile';
import QuickActionButton from '../../src/components/QuickActionButton';
import SystemChart from '../../src/components/SystemChart';

import { useMeshStore } from '../../src/store/useMeshStore';

export default function DashboardScreen() {
  const router = useRouter();

  const [uptimeMs, setUptimeMs] = useState(0);

  useEffect(() => {
    const start = Date.now();
    const interval = setInterval(() => {
      setUptimeMs(Date.now() - start);
    }, 1000);
    return () => clearInterval(interval);
  }, []);

  const nodesRecord = useMeshStore((s) => s.nodes);
  const wsStatus = useMeshStore((s) => s.wsStatus);
  const eventLog = useMeshStore((s) => s.eventLog);
  const allNodes = Object.values(nodesRecord);
  const onlineCount = allNodes.filter(n => n.status === 'Online').length;
  const totalCount = allNodes.length;

  // Calculate active sensors
  let activeSensorsCount = 0;
  allNodes.filter(n => n.status === 'Online').forEach(n => {
    const names = new Set();
    n.sensorEntries?.forEach((e: any) => {
      if (e.sensorName) names.add(e.sensorName);
    });
    activeSensorsCount += names.size;
  });

  // Calculate Last Seen
  let lastSeenMax = 0;
  allNodes.forEach(n => {
    if (n.lastSeen && n.lastSeen > lastSeenMax) {
      lastSeenMax = n.lastSeen;
    }
  });
  const lastSeenStr = lastSeenMax > 0 ? new Date(lastSeenMax).toLocaleTimeString() : '--:--:--';

  const errorsCount = allNodes.reduce((acc, n) => acc + (n.runtimeErrors?.length || 0), 0);

  const formatUptime = (ms: number) => {
    const totalSeconds = Math.floor(ms / 1000);
    const h = Math.floor(totalSeconds / 3600);
    const m = Math.floor((totalSeconds % 3600) / 60);
    const s = totalSeconds % 60;
    return `${h > 0 ? h + 'h ' : ''}${m}m ${s}s`;
  };

  const isHealthy = errorsCount === 0 && wsStatus === 'Connected';
  const HealthIcon = isHealthy ? ShieldCheck : ShieldAlert;
  const healthColor = isHealthy ? COLORS.success : COLORS.warning;

  return (
    <SafeAreaView style={styles.container} edges={['top']}>
      
      {/* Header */}
      <View style={styles.header}>
        <View style={styles.headerLeft}>
          <View>
            <Text style={styles.headerTitle}>Dashboard</Text>
            <Text style={styles.headerSubtitle}>Overview of your ESP network</Text>
          </View>
        </View>
        <TouchableOpacity 
          style={styles.bellButton}
          onPress={() => Alert.alert('Thông báo', 'Hệ thống đã hoạt động bình thường')}
        >
          <Bell size={20} color={COLORS.textMain} />
          <View style={styles.bellBadge} />
        </TouchableOpacity>
      </View>

      <ScrollView contentContainerStyle={styles.scrollContent} showsVerticalScrollIndicator={false}>
        
        {/* Root Status Hero Card */}
        <LinearGradient
          colors={['#101633', '#151e45']}
          style={styles.heroCard}
          start={{ x: 0, y: 0 }}
          end={{ x: 1, y: 1 }}
        >
          <View style={styles.heroContent}>
            <View style={styles.wifiContainer}>
              <View style={styles.wifiGlow}>
                <Wifi size={32} color="#fff" />
              </View>
            </View>
            
            <View style={styles.heroTextContent}>
              <Text style={styles.heroLabel}>Server Status</Text>
              <View style={styles.heroStatusRow}>
                <Text style={styles.heroValue}>{wsStatus}</Text>
                <View style={styles.heroBadge}>
                  <Text style={styles.heroBadgeText}>WebSocket</Text>
                </View>
              </View>
              <Text style={styles.heroDetail}>Connection established and active</Text>
            </View>

            {/* Placeholder for ESP Chip */}
            <View style={styles.espChipPlaceholder}>
              <View style={styles.chipBody}>
                <Text style={styles.chipText}>ESP</Text>
              </View>
              <View style={[styles.chipPin, {top: 10, left: -4}]} />
              <View style={[styles.chipPin, {top: 20, left: -4}]} />
              <View style={[styles.chipPin, {top: 30, left: -4}]} />
              <View style={[styles.chipPin, {top: 10, right: -4}]} />
              <View style={[styles.chipPin, {top: 20, right: -4}]} />
              <View style={[styles.chipPin, {top: 30, right: -4}]} />
              <View style={styles.chipGlow} />
            </View>
            
            <View style={styles.heroDot} />
          </View>
        </LinearGradient>

        {/* Metrics Grid */}
        <View style={styles.grid}>
          <MetricTile
            title="Nodes"
            value={totalCount > 0 ? `${onlineCount} / ${totalCount}` : '0 / 0'}
            detail={totalCount > 0 ? "Network active" : "Waiting for nodes"}
            accentColor={COLORS.info}
            icon={<Server size={18} color="#fff" />}
            onPress={() => router.push('/(tabs)/nodes')}
          />
          <MetricTile
            title="Sensors"
            value={activeSensorsCount.toString()}
            detail="Active readers"
            accentColor={COLORS.success}
            icon={<Activity size={18} color="#fff" />}
            onPress={() => router.push('/(tabs)/sensors')}
          />
          <MetricTile
            title="Last Seen"
            value={lastSeenStr}
            detail="Latest data received"
            accentColor={COLORS.primary}
            icon={<Clock size={18} color="#fff" />}
          />
          <MetricTile
            title="Errors"
            value={errorsCount.toString()}
            detail={errorsCount === 0 ? "All clear" : "Check issues!"}
            accentColor={errorsCount > 0 ? COLORS.error : COLORS.warning}
            icon={<AlertTriangle size={18} color="#fff" />}
          />
        </View>

        {/* System Chart */}
        <SystemChart />

        {/* Quick Actions */}
        <View style={styles.quickActionsSection}>
          <View style={styles.sectionHeaderRow}>
            <Activity size={20} color={COLORS.primary} />
            <View>
              <Text style={styles.sectionTitle}>Quick Actions</Text>
              <Text style={styles.sectionSubtitle}>Common actions and shortcuts</Text>
            </View>
          </View>
          
          <ScrollView horizontal showsHorizontalScrollIndicator={false} contentContainerStyle={styles.quickActionsScroll}>
            <QuickActionButton 
              icon={<Server size={20} color={COLORS.info} />}
              title="Nodes"
              subtitle="View all nodes"
              onPress={() => router.push('/(tabs)/nodes')}
            />
            <QuickActionButton 
              icon={<Activity size={20} color={COLORS.success} />}
              title="Sensors"
              subtitle="View all sensors"
              onPress={() => router.push('/(tabs)/sensors')}
            />
            <QuickActionButton 
              icon={<History size={20} color={COLORS.primary} />}
              title="History"
              subtitle="View history"
              onPress={() => router.push('/(tabs)/history')}
            />
            <QuickActionButton 
              icon={<Settings size={20} color={COLORS.secondary} />}
              title="Settings"
              subtitle="Configure system"
              onPress={() => router.push('/(tabs)/settings')}
            />
          </ScrollView>
        </View>

        {/* System Healthy Footer */}
        <View style={[styles.footerBanner, { borderColor: isHealthy ? 'transparent' : COLORS.warning, borderWidth: isHealthy ? 0 : 1 }]}>
          <View style={styles.footerLeft}>
            <View style={[styles.shieldContainer, { backgroundColor: isHealthy ? COLORS.successDark : 'rgba(255, 171, 0, 0.2)' }]}>
              <HealthIcon size={24} color={healthColor} />
            </View>
            <View>
              <Text style={[styles.footerTitle, { color: healthColor }]}>
                {isHealthy ? "System Healthy" : "Issues Detected"}
              </Text>
              <Text style={styles.footerSubtitle}>
                {isHealthy ? "All systems operational" : `${errorsCount} error(s) found`}
              </Text>
            </View>
          </View>
          <View style={styles.footerRight}>
            <Text style={styles.footerUptimeLabel}>App Uptime</Text>
            <Text style={styles.footerUptime}>{formatUptime(uptimeMs)}</Text>
          </View>
        </View>

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
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: SIZES.large,
    paddingVertical: SIZES.medium,
  },
  headerLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: SIZES.medium,
  },
  menuButton: {
    padding: 4,
  },
  headerTitle: {
    fontSize: SIZES.extraLarge,
    fontWeight: 'bold',
    color: COLORS.textMain,
  },
  headerSubtitle: {
    fontSize: SIZES.small,
    color: COLORS.secondary,
  },
  bellButton: {
    padding: 8,
    position: 'relative',
  },
  bellBadge: {
    position: 'absolute',
    top: 6,
    right: 8,
    width: 8,
    height: 8,
    borderRadius: 4,
    backgroundColor: COLORS.success,
    borderWidth: 1.5,
    borderColor: COLORS.background,
  },
  scrollContent: {
    padding: SIZES.large,
    paddingBottom: 40,
  },
  heroCard: {
    borderRadius: SIZES.cardRadius,
    padding: SIZES.large,
    marginBottom: SIZES.large,
    borderWidth: 1,
    borderColor: COLORS.border,
    overflow: 'hidden',
  },
  heroContent: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  wifiContainer: {
    width: 64,
    height: 64,
    borderRadius: 32,
    backgroundColor: 'rgba(0, 117, 255, 0.1)',
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: SIZES.large,
  },
  wifiGlow: {
    width: 48,
    height: 48,
    borderRadius: 24,
    backgroundColor: COLORS.primary,
    justifyContent: 'center',
    alignItems: 'center',
    shadowColor: COLORS.primary,
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 0.8,
    shadowRadius: 15,
    elevation: 10,
  },
  heroTextContent: {
    flex: 1,
    zIndex: 2,
  },
  heroLabel: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
    marginBottom: 4,
  },
  heroStatusRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: SIZES.small,
    marginBottom: 4,
  },
  heroValue: {
    color: COLORS.textMain,
    fontSize: SIZES.extraLarge,
    fontWeight: 'bold',
  },
  heroBadge: {
    backgroundColor: COLORS.successDark,
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 12,
  },
  heroBadgeText: {
    color: COLORS.success,
    fontSize: 10,
    fontWeight: 'bold',
  },
  heroDetail: {
    color: COLORS.secondary,
    fontSize: SIZES.small - 1,
  },
  espChipPlaceholder: {
    position: 'absolute',
    right: -10,
    top: -10,
    width: 80,
    height: 60,
    justifyContent: 'center',
    alignItems: 'center',
    opacity: 0.8,
    transform: [{ rotate: '15deg' }],
  },
  chipBody: {
    width: 50,
    height: 40,
    backgroundColor: '#0a0e29',
    borderRadius: 4,
    borderWidth: 1,
    borderColor: COLORS.primary,
    justifyContent: 'center',
    alignItems: 'center',
    zIndex: 2,
  },
  chipText: {
    color: COLORS.primary,
    fontSize: 10,
    fontWeight: 'bold',
  },
  chipPin: {
    position: 'absolute',
    width: 8,
    height: 4,
    backgroundColor: COLORS.secondary,
    borderRadius: 2,
  },
  chipGlow: {
    position: 'absolute',
    width: 100,
    height: 100,
    borderRadius: 50,
    backgroundColor: COLORS.primary,
    opacity: 0.1,
    zIndex: 1,
  },
  heroDot: {
    position: 'absolute',
    top: 0,
    right: 0,
    width: 8,
    height: 8,
    borderRadius: 4,
    backgroundColor: COLORS.success,
    shadowColor: COLORS.success,
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 1,
    shadowRadius: 6,
  },
  grid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'space-between',
    marginBottom: SIZES.base,
  },
  quickActionsSection: {
    marginBottom: SIZES.large,
  },
  sectionHeaderRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: SIZES.small,
    marginBottom: SIZES.medium,
  },
  sectionTitle: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: 'bold',
  },
  sectionSubtitle: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  quickActionsScroll: {
    paddingRight: SIZES.large,
  },
  footerBanner: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.radius,
    padding: SIZES.medium,
  },
  footerLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: SIZES.medium,
  },
  shieldContainer: {
    width: 36,
    height: 36,
    borderRadius: 18,
    backgroundColor: COLORS.successDark,
    justifyContent: 'center',
    alignItems: 'center',
  },
  footerTitle: {
    color: COLORS.success,
    fontSize: SIZES.font,
    fontWeight: 'bold',
  },
  footerSubtitle: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  footerRight: {
    alignItems: 'flex-end',
  },
  footerUptimeLabel: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  footerUptime: {
    color: COLORS.textMain,
    fontSize: SIZES.font,
    fontWeight: 'bold',
  }
});
