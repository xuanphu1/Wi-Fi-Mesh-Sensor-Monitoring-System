import React from 'react';
import { View, Text, StyleSheet, ScrollView, Dimensions } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { LineChart } from 'react-native-gifted-charts';
import { Cpu, Database, Battery, Wifi, Clock, Thermometer, Zap, AlertTriangle, Activity } from 'lucide-react-native';
import { COLORS, SIZES } from '../../src/constants/theme';
import { useMeshStore } from '../../src/store/useMeshStore';

const formatUptime = (seconds: number) => {
  const d = Math.floor(seconds / (3600 * 24));
  const h = Math.floor((seconds % (3600 * 24)) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = Math.floor(seconds % 60);

  if (d > 0) return `${d}d ${h}h`;
  if (h > 0) return `${h}h ${m}m`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
};

export default function GatewayScreen() {
  const gateway = useMeshStore(state => state.gateway);
  const isOnline = useMeshStore(state => state.isGatewayOnline());
  const gatewaySeries = useMeshStore(state => state.gatewaySeries);

  const getBatteryColor = (percent: number) => {
    if (percent > 50) return COLORS.success;
    if (percent > 20) return COLORS.warning;
    return COLORS.error;
  };

  const getWifiColor = (rssi: number) => {
    if (rssi >= -50) return COLORS.success;
    if (rssi >= -70) return COLORS.warning;
    return COLORS.error;
  };

  return (
    <SafeAreaView style={styles.container} edges={['top']}>
      {/* Header */}
      <View style={styles.header}>
        <View style={styles.headerTitleContainer}>
          <Text style={styles.headerTitle}>Gateway</Text>
          <Text style={styles.headerSubtitle}>Core system metrics</Text>
        </View>
        <View style={[styles.statusBadge, { backgroundColor: isOnline ? COLORS.successDark : COLORS.warningDark }]}>
          <View style={[styles.statusDot, { backgroundColor: isOnline ? COLORS.success : COLORS.warning }]} />
          <Text style={[styles.statusText, { color: isOnline ? COLORS.success : COLORS.warning }]}>
            {isOnline ? 'Online' : 'Offline'}
          </Text>
        </View>
      </View>

      <ScrollView contentContainerStyle={styles.scrollContent} showsVerticalScrollIndicator={false}>
        {!gateway ? (
          <View style={styles.emptyState}>
            <Activity size={48} color={COLORS.secondary} />
            <Text style={styles.emptyText}>Waiting for Gateway status...</Text>
          </View>
        ) : (
          <View>
            {/* Chart Section */}
            <View style={styles.chartCard}>
              <View style={styles.cardHeader}>
                <View style={[styles.iconBox, { backgroundColor: 'rgba(0, 117, 255, 0.1)' }]}>
                  <Cpu size={20} color={COLORS.primary} />
                </View>
                <Text style={styles.cardTitle}>CPU Load History (%)</Text>
              </View>
              <LineChart
                data={gatewaySeries.length > 0 ? gatewaySeries.map(s => ({ value: s.cpu || 0 })) : [{ value: 0 }]}
                width={Dimensions.get('window').width - (SIZES.large * 2) - 60}
                height={120}
                thickness={2}
                color={COLORS.primary}
                hideDataPoints
                startFillColor={COLORS.primary}
                endFillColor={COLORS.primary}
                startOpacity={0.3}
                endOpacity={0.0}
                yAxisColor={COLORS.border}
                xAxisColor={COLORS.border}
                yAxisTextStyle={{ color: COLORS.secondary, fontSize: 10 }}
                rulesColor={COLORS.border}
                rulesType="dashed"
                maxValue={100}
                noOfSections={4}
                stepValue={25}
                areaChart
                curved
                isAnimated
                animationDuration={800}
                initialSpacing={0}
                spacing={(Dimensions.get('window').width - (SIZES.large * 2) - 60) / Math.max(1, gatewaySeries.length - 1)}
                yAxisLabelWidth={30}
              />
            </View>

            <View style={styles.grid}>
              {/* CPU */}
            <View style={styles.card}>
              <View style={styles.cardHeader}>
                <View style={[styles.iconBox, { backgroundColor: 'rgba(0, 117, 255, 0.1)' }]}>
                  <Cpu size={20} color={COLORS.primary} />
                </View>
                <Text style={styles.cardTitle}>CPU Load</Text>
              </View>
              <Text style={styles.cardValue}>{gateway.cpu_load_percent?.toFixed(1) || 0}%</Text>
              <View style={styles.progressBarBg}>
                <View style={[styles.progressBarFill, { width: `${Math.min(100, gateway.cpu_load_percent || 0)}%`, backgroundColor: COLORS.primary }]} />
              </View>
            </View>

            {/* RAM */}
            <View style={styles.card}>
              <View style={styles.cardHeader}>
                <View style={[styles.iconBox, { backgroundColor: 'rgba(138, 43, 226, 0.1)' }]}>
                  <Database size={20} color={COLORS.info} />
                </View>
                <Text style={styles.cardTitle}>RAM Usage</Text>
              </View>
              <Text style={styles.cardValue}>{gateway.ram_used_percent?.toFixed(1) || 0}%</Text>
              <Text style={styles.cardSubValue}>{gateway.ram_used_kb || 0} KB used</Text>
              <View style={styles.progressBarBg}>
                <View style={[styles.progressBarFill, { width: `${Math.min(100, gateway.ram_used_percent || 0)}%`, backgroundColor: COLORS.info }]} />
              </View>
            </View>

            {/* Battery */}
            <View style={styles.card}>
              <View style={styles.cardHeader}>
                <View style={[styles.iconBox, { backgroundColor: 'rgba(1, 247, 167, 0.1)' }]}>
                  <Battery size={20} color={COLORS.success} />
                </View>
                <Text style={styles.cardTitle}>Power</Text>
              </View>
              <Text style={styles.cardValue}>{gateway.battery_percent?.toFixed(0) || 0}%</Text>
              <Text style={styles.cardSubValue}>{gateway.battery_voltage_v?.toFixed(2) || 0}V • {gateway.power_source}</Text>
              <View style={styles.progressBarBg}>
                <View style={[styles.progressBarFill, { width: `${Math.min(100, gateway.battery_percent || 0)}%`, backgroundColor: getBatteryColor(gateway.battery_percent || 0) }]} />
              </View>
            </View>

            {/* Network */}
            <View style={styles.card}>
              <View style={styles.cardHeader}>
                <View style={[styles.iconBox, { backgroundColor: 'rgba(255, 181, 71, 0.1)' }]}>
                  <Wifi size={20} color={COLORS.warning} />
                </View>
                <Text style={styles.cardTitle}>Network</Text>
              </View>
              <Text style={styles.cardValue}>{gateway.wifi_rssi || 0} dBm</Text>
              <Text style={styles.cardSubValue}>{gateway.wifi_ssid || 'Unknown'}</Text>
              <Text style={styles.cardSubValue}>{gateway.sta_ip || 'No IP'}</Text>
            </View>

            {/* Temperature */}
            {gateway.chip_temp_internal_supported && (
              <View style={styles.card}>
                <View style={styles.cardHeader}>
                  <View style={[styles.iconBox, { backgroundColor: 'rgba(255, 40, 92, 0.1)' }]}>
                    <Thermometer size={20} color={COLORS.error} />
                  </View>
                  <Text style={styles.cardTitle}>Chip Temp</Text>
                </View>
                <Text style={styles.cardValue}>{gateway.chip_temp_c !== null ? `${gateway.chip_temp_c.toFixed(1)}°C` : 'N/A'}</Text>
              </View>
            )}

            {/* Uptime */}
            <View style={styles.card}>
              <View style={styles.cardHeader}>
                <View style={[styles.iconBox, { backgroundColor: 'rgba(255, 255, 255, 0.1)' }]}>
                  <Clock size={20} color={COLORS.textMain} />
                </View>
                <Text style={styles.cardTitle}>Uptime</Text>
              </View>
              <Text style={styles.cardValue}>{formatUptime(gateway.uptime_s || 0)}</Text>
            </View>
            </View>
          </View>
        )}
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
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: SIZES.large,
    paddingTop: SIZES.medium,
    paddingBottom: SIZES.medium,
  },
  headerTitleContainer: {
    flex: 1,
  },
  headerTitle: {
    fontSize: SIZES.extraLarge,
    fontWeight: 'bold',
    color: COLORS.textMain,
  },
  headerSubtitle: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
    marginTop: 2,
  },
  statusBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 20,
    gap: 6,
  },
  statusDot: {
    width: 8,
    height: 8,
    borderRadius: 4,
  },
  statusText: {
    fontSize: SIZES.small,
    fontWeight: '600',
  },
  scrollContent: {
    padding: SIZES.large,
    paddingBottom: 100,
  },
  emptyState: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: 100,
  },
  emptyText: {
    color: COLORS.secondary,
    marginTop: SIZES.medium,
    fontSize: SIZES.font,
  },
  grid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: SIZES.medium,
    justifyContent: 'space-between',
  },
  chartCard: {
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.cardRadius,
    padding: SIZES.medium,
    borderWidth: 1,
    borderColor: COLORS.border,
    marginBottom: SIZES.medium,
  },
  card: {
    width: '47%',
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.cardRadius,
    padding: SIZES.medium,
    borderWidth: 1,
    borderColor: COLORS.border,
    marginBottom: SIZES.small,
  },
  cardHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    marginBottom: 12,
  },
  iconBox: {
    width: 32,
    height: 32,
    borderRadius: 8,
    alignItems: 'center',
    justifyContent: 'center',
  },
  cardTitle: {
    color: COLORS.secondary,
    fontSize: SIZES.font,
    fontWeight: '500',
  },
  cardValue: {
    color: COLORS.textMain,
    fontSize: 24,
    fontWeight: 'bold',
    marginBottom: 4,
  },
  cardSubValue: {
    color: COLORS.secondary,
    fontSize: 12,
    marginBottom: 4,
  },
  progressBarBg: {
    height: 4,
    backgroundColor: COLORS.surfaceLight,
    borderRadius: 2,
    marginTop: 8,
    overflow: 'hidden',
  },
  progressBarFill: {
    height: '100%',
    borderRadius: 2,
  },
});
