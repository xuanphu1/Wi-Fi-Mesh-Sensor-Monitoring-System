import React from 'react';
import { View, Text, StyleSheet, ScrollView, Dimensions } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { LineChart } from 'react-native-gifted-charts';
import { Cpu, MemoryStick, Clock, Thermometer, Activity, HardDrive } from 'lucide-react-native';
import { COLORS, SIZES } from '../../src/constants/theme';
import { useMeshStore } from '../../src/store/useMeshStore';

const formatUptime = (seconds: number) => {
  if (!seconds) return '0s';
  const d = Math.floor(seconds / (3600 * 24));
  const h = Math.floor((seconds % (3600 * 24)) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = Math.floor(seconds % 60);

  if (d > 0) return `${d}d ${h}h`;
  if (h > 0) return `${h}h ${m}m`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
};

export default function ServerScreen() {
  const serverMetrics = useMeshStore(state => state.serverMetrics);
  const serverMetricsSeries = useMeshStore(state => state.serverMetricsSeries);

  const isOnline = serverMetrics && (Date.now() - new Date(serverMetrics.receivedAtIso).getTime() < 30000);

  return (
    <SafeAreaView style={styles.container} edges={['top']}>
      {/* Header */}
      <View style={styles.header}>
        <View style={styles.headerTitleContainer}>
          <Text style={styles.headerTitle}>Server Monitor</Text>
          <Text style={styles.headerSubtitle}>Core system metrics</Text>
        </View>
        <View style={[styles.statusBadge, { backgroundColor: isOnline ? COLORS.successDark : COLORS.errorDark }]}>
          <View style={[styles.statusDot, { backgroundColor: isOnline ? COLORS.success : COLORS.error }]} />
          <Text style={[styles.statusText, { color: isOnline ? COLORS.success : COLORS.error }]}>
            {isOnline ? 'Online' : 'Offline'}
          </Text>
        </View>
      </View>

      <ScrollView contentContainerStyle={styles.scrollContent} showsVerticalScrollIndicator={false}>
        {!serverMetrics ? (
          <View style={styles.emptyState}>
            <Activity size={48} color={COLORS.secondary} />
            <Text style={styles.emptyText}>Waiting for Server status...</Text>
          </View>
        ) : (
          <View>
            {/* Chart Section */}
            <View style={styles.chartCard}>
              <View style={styles.cardHeader}>
                <View style={[styles.iconBox, { backgroundColor: 'rgba(0, 117, 255, 0.1)' }]}>
                  <Cpu size={20} color={COLORS.primary} />
                </View>
                <Text style={styles.cardTitle}>CPU & RAM History (%)</Text>
              </View>
              
              <View style={styles.chartLegend}>
                <View style={styles.legendItem}>
                  <View style={[styles.legendColor, { backgroundColor: COLORS.primary }]} />
                  <Text style={styles.legendText}>CPU</Text>
                </View>
                <View style={styles.legendItem}>
                  <View style={[styles.legendColor, { backgroundColor: COLORS.warning }]} />
                  <Text style={styles.legendText}>RAM</Text>
                </View>
              </View>

              <LineChart
                data={serverMetricsSeries.length > 0 ? serverMetricsSeries.map(s => ({ value: s.cpu || 0 })) : [{ value: 0 }]}
                data2={serverMetricsSeries.length > 0 ? serverMetricsSeries.map(s => ({ value: s.ram || 0 })) : [{ value: 0 }]}
                width={Dimensions.get('window').width - (SIZES.large * 2) - 60}
                height={120}
                thickness={2}
                thickness2={2}
                color={COLORS.primary}
                color2={COLORS.warning}
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
                spacing={(Dimensions.get('window').width - (SIZES.large * 2) - 60) / Math.max(1, serverMetricsSeries.length - 1)}
                yAxisLabelWidth={30}
              />
            </View>

            <View style={styles.grid}>
              {/* CPU */}
              <View style={styles.metricCard}>
                <View style={styles.metricHeader}>
                  <Cpu size={18} color={COLORS.secondary} />
                  <Text style={styles.metricLabel}>CPU Load</Text>
                </View>
                <Text style={[styles.metricValue, { color: serverMetrics.cpuLoadPercent > 85 ? COLORS.error : COLORS.textMain }]}>
                  {serverMetrics.cpuLoadPercent?.toFixed(1)}%
                </Text>
              </View>

              {/* RAM */}
              <View style={styles.metricCard}>
                <View style={styles.metricHeader}>
                  <HardDrive size={18} color={COLORS.secondary} />
                  <Text style={styles.metricLabel}>RAM Usage</Text>
                </View>
                <Text style={[styles.metricValue, { color: serverMetrics.ramUsedPercent > 85 ? COLORS.error : COLORS.textMain }]}>
                  {serverMetrics.ramUsedPercent?.toFixed(1)}%
                </Text>
                <Text style={styles.metricSub}>
                  {serverMetrics.ramUsedMb?.toFixed(0)} / {serverMetrics.ramTotalMb?.toFixed(0)} MB
                </Text>
              </View>

              {/* Temp */}
              <View style={styles.metricCard}>
                <View style={styles.metricHeader}>
                  <Thermometer size={18} color={COLORS.secondary} />
                  <Text style={styles.metricLabel}>Temperature</Text>
                </View>
                <Text style={[styles.metricValue, { color: serverMetrics.chipTempC > 75 ? COLORS.error : COLORS.textMain }]}>
                  {serverMetrics.chipTempC?.toFixed(1)}°C
                </Text>
              </View>

              {/* Uptime */}
              <View style={styles.metricCard}>
                <View style={styles.metricHeader}>
                  <Clock size={18} color={COLORS.secondary} />
                  <Text style={styles.metricLabel}>Uptime</Text>
                </View>
                <Text style={styles.metricValue}>
                  {formatUptime(serverMetrics.uptimeS)}
                </Text>
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
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: SIZES.large,
    paddingVertical: SIZES.medium,
    backgroundColor: COLORS.surface,
    borderBottomWidth: 1,
    borderBottomColor: COLORS.border,
  },
  headerTitleContainer: {
    flex: 1,
  },
  headerTitle: {
    fontSize: 22,
    fontWeight: 'bold',
    color: COLORS.textMain,
  },
  headerSubtitle: {
    fontSize: 14,
    color: COLORS.secondary,
    marginTop: 2,
  },
  statusBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 16,
    borderWidth: 1,
    borderColor: 'rgba(255,255,255,0.1)',
  },
  statusDot: {
    width: 8,
    height: 8,
    borderRadius: 4,
    marginRight: 6,
  },
  statusText: {
    fontSize: 12,
    fontWeight: '600',
  },
  scrollContent: {
    padding: SIZES.large,
  },
  emptyState: {
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: 60,
  },
  emptyText: {
    marginTop: 16,
    fontSize: 16,
    color: COLORS.secondary,
  },
  chartCard: {
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.radius,
    padding: SIZES.medium,
    marginBottom: SIZES.large,
    borderWidth: 1,
    borderColor: COLORS.border,
    overflow: 'hidden',
  },
  cardHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: SIZES.large,
  },
  iconBox: {
    width: 36,
    height: 36,
    borderRadius: 18,
    alignItems: 'center',
    justifyContent: 'center',
    marginRight: 12,
  },
  cardTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: COLORS.textMain,
  },
  chartLegend: {
    flexDirection: 'row',
    justifyContent: 'flex-end',
    marginBottom: 8,
  },
  legendItem: {
    flexDirection: 'row',
    alignItems: 'center',
    marginLeft: 16,
  },
  legendColor: {
    width: 8,
    height: 8,
    borderRadius: 4,
    marginRight: 6,
  },
  legendText: {
    fontSize: 12,
    color: COLORS.secondary,
  },
  grid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'space-between',
  },
  metricCard: {
    width: '48%',
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.radius,
    padding: SIZES.medium,
    marginBottom: SIZES.medium,
    borderWidth: 1,
    borderColor: COLORS.border,
  },
  metricHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 12,
  },
  metricLabel: {
    fontSize: 14,
    color: COLORS.secondary,
    marginLeft: 8,
  },
  metricValue: {
    fontSize: 24,
    fontWeight: 'bold',
    color: COLORS.textMain,
  },
  metricSub: {
    fontSize: 12,
    color: COLORS.secondary,
    marginTop: 4,
  },
});
