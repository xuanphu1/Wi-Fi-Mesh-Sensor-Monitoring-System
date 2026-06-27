import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity } from 'react-native';
import { Cpu, ChevronRight, Copy, MapPin, Clock } from 'lucide-react-native';
import { COLORS, SIZES } from '../constants/theme';
import MiniMetricCard from './MiniMetricCard';

export interface MetricData {
  title: string;
  value: string | number;
  unit: string;
  icon: React.ReactNode;
  colorHex: string;
  mockPoints: number[];
}

interface SensorCardProps {
  sensorName: string;
  ip: string;
  location: string;
  lastUpdate: string;
  status: 'Online' | 'Warning' | 'Offline';
  metrics: MetricData[];
}

export default function SensorCard({ sensorName, ip, location, lastUpdate, status, metrics }: SensorCardProps) {
  let statusColor = COLORS.success;
  let statusBg = `${COLORS.success}15`;
  if (status === 'Warning') {
    statusColor = COLORS.warning;
    statusBg = `${COLORS.warning}15`;
  } else if (status === 'Offline') {
    statusColor = COLORS.error;
    statusBg = `${COLORS.error}15`;
  }

  // Choose a base icon color based on the sensor name
  let iconColor = '#8a2be2'; // Purple
  if (sensorName.includes('AHT')) iconColor = COLORS.info;
  if (sensorName.includes('DHT')) iconColor = COLORS.success;
  if (sensorName.includes('BME680')) iconColor = COLORS.warning;

  return (
    <View style={styles.card}>
      {/* Header Section */}
      <View style={styles.header}>
        <View style={styles.headerLeft}>
          <View style={[styles.iconContainer, { backgroundColor: `${iconColor}20` }]}>
            <Cpu size={24} color={iconColor} />
          </View>
          
          <View style={styles.infoContainer}>
            <View style={styles.titleRow}>
              <Text style={styles.sensorName}>{sensorName}</Text>
              <View style={[styles.statusBadge, { backgroundColor: statusBg }]}>
                <View style={[styles.statusDot, { backgroundColor: statusColor }]} />
                <Text style={[styles.statusText, { color: statusColor }]}>{status}</Text>
              </View>
            </View>
            
            <View style={styles.ipRow}>
              <Text style={styles.ipText}>{ip}</Text>
              <TouchableOpacity style={styles.copyButton}>
                <Copy size={12} color={COLORS.info} />
              </TouchableOpacity>
            </View>
            
            <View style={styles.metaRow}>
              <View style={styles.metaItem}>
                <MapPin size={12} color={COLORS.secondary} />
                <Text style={styles.metaText}>{location}</Text>
              </View>
            </View>
            <View style={styles.metaRow}>
              <View style={styles.metaItem}>
                <Clock size={12} color={COLORS.secondary} />
                <Text style={styles.metaText}>Last update: {lastUpdate}</Text>
              </View>
            </View>
          </View>
        </View>

        <TouchableOpacity style={styles.headerRight}>
          <ChevronRight size={20} color={COLORS.secondary} />
        </TouchableOpacity>
      </View>

      {/* Metrics Grid */}
      <View style={styles.metricsGrid}>
        {metrics.map((m, index) => (
          <MiniMetricCard
            key={index}
            icon={m.icon}
            title={m.title}
            value={m.value}
            unit={m.unit}
            colorHex={m.colorHex}
            mockPoints={m.mockPoints}
          />
        ))}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.radius,
    marginBottom: SIZES.large,
    padding: SIZES.large,
    borderWidth: 1,
    borderColor: COLORS.border,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'flex-start',
    marginBottom: SIZES.medium,
  },
  headerLeft: {
    flexDirection: 'row',
    flex: 1,
  },
  iconContainer: {
    width: 48,
    height: 48,
    borderRadius: 12,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: SIZES.medium,
  },
  infoContainer: {
    flex: 1,
  },
  titleRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: SIZES.small,
    marginBottom: 4,
  },
  sensorName: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: 'bold',
  },
  statusBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 12,
    gap: 4,
  },
  statusDot: {
    width: 6,
    height: 6,
    borderRadius: 3,
  },
  statusText: {
    fontSize: 10,
    fontWeight: '500',
  },
  ipRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    marginBottom: 4,
  },
  ipText: {
    color: COLORS.info,
    fontSize: SIZES.font,
    fontWeight: '500',
  },
  copyButton: {
    padding: 2,
  },
  metaRow: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 2,
  },
  metaItem: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
  },
  metaText: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  headerRight: {
    padding: 4,
  },
  metricsGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'space-between',
  }
});
