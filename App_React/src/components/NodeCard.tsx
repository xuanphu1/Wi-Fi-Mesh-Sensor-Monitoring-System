import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity } from 'react-native';
import { Cpu, Wifi, ChevronRight } from 'lucide-react-native';
import { COLORS, SIZES } from '../constants/theme';

interface NodeCardProps {
  name: string;
  boardType: string;
  ip: string;
  rssi: string;
  status: 'Online' | 'Offline';
  isMain?: boolean;
  colorHex: string;
  onPress?: () => void;
}

export default function NodeCard({ name, boardType, ip, rssi, status, isMain, colorHex, onPress }: NodeCardProps) {
  const isOnline = status === 'Online';
  const statusColor = isOnline ? COLORS.success : COLORS.error;

  return (
    <TouchableOpacity 
      style={styles.card} 
      activeOpacity={onPress ? 0.7 : 1}
      onPress={onPress}
    >
      <View style={styles.content}>
        {/* Left Icon */}
        <View style={[styles.iconContainer, { backgroundColor: `${colorHex}15` }]}>
          <Cpu size={24} color={colorHex} />
        </View>

        {/* Center Details */}
        <View style={styles.detailsContainer}>
          <View style={styles.nameRow}>
            <Text style={styles.name} numberOfLines={1}>{name}</Text>
            {isMain && (
              <View style={styles.mainBadge}>
                <Text style={styles.mainBadgeText}>Main</Text>
              </View>
            )}
          </View>
          <Text style={styles.boardType}>{boardType}</Text>
          
          <View style={styles.networkRow}>
            <Wifi size={12} color={COLORS.secondary} />
            <Text style={styles.ipText}>{ip}</Text>
          </View>
        </View>

        {/* Right Status */}
        <View style={styles.rightContainer}>
          <View style={[styles.statusBadge, { borderColor: statusColor, backgroundColor: `${statusColor}15` }]}>
            <Text style={[styles.statusText, { color: statusColor }]}>{status}</Text>
          </View>
          
          <View style={styles.rssiRow}>
            {/* Simple signal bars */}
            <View style={styles.signalBars}>
              <View style={[styles.bar, styles.bar1, isOnline && {backgroundColor: COLORS.secondary}]} />
              <View style={[styles.bar, styles.bar2, isOnline && {backgroundColor: COLORS.secondary}]} />
              <View style={[styles.bar, styles.bar3, isOnline && {backgroundColor: COLORS.secondary}]} />
              <View style={[styles.bar, styles.bar4, isOnline && {backgroundColor: COLORS.secondary}]} />
            </View>
            <Text style={styles.rssiText}>{isOnline ? rssi : '--'}</Text>
          </View>
        </View>

        {/* Chevron */}
        <View style={styles.chevronContainer}>
          <ChevronRight size={20} color={COLORS.secondary} />
        </View>
      </View>
    </TouchableOpacity>
  );
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.radius,
    marginBottom: SIZES.medium,
    padding: SIZES.large,
    borderWidth: 1,
    borderColor: COLORS.border,
  },
  content: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  iconContainer: {
    width: 48,
    height: 48,
    borderRadius: 12,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: SIZES.medium,
  },
  detailsContainer: {
    flex: 1,
    justifyContent: 'center',
  },
  nameRow: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 2,
    gap: SIZES.small,
  },
  name: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: 'bold',
  },
  mainBadge: {
    backgroundColor: 'rgba(138, 43, 226, 0.2)', // Purple tint
    paddingHorizontal: 6,
    paddingVertical: 2,
    borderRadius: 4,
  },
  mainBadgeText: {
    color: '#a875ff',
    fontSize: 10,
    fontWeight: 'bold',
  },
  boardType: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
    marginBottom: 4,
  },
  networkRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
  },
  ipText: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  rightContainer: {
    alignItems: 'flex-end',
    justifyContent: 'space-between',
    height: 44,
    marginRight: SIZES.small,
  },
  statusBadge: {
    borderWidth: 1,
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 12,
  },
  statusText: {
    fontSize: 10,
    fontWeight: 'bold',
  },
  rssiRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
  },
  signalBars: {
    flexDirection: 'row',
    alignItems: 'flex-end',
    height: 12,
    gap: 2,
  },
  bar: {
    width: 2,
    backgroundColor: COLORS.border,
    borderRadius: 1,
  },
  bar1: { height: 4 },
  bar2: { height: 6 },
  bar3: { height: 9 },
  bar4: { height: 12 },
  rssiText: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  chevronContainer: {
    justifyContent: 'center',
  }
});
