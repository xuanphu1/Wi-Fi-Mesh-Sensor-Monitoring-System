import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity } from 'react-native';
import { ChevronRight } from 'lucide-react-native';
import { COLORS, SIZES } from '../constants/theme';

interface MetricTileProps {
  title: string;
  value: string | number;
  detail: string;
  accentColor: string;
  icon: React.ReactNode;
  onPress?: () => void;
}

export default function MetricTile({ title, value, detail, accentColor, icon, onPress }: MetricTileProps) {
  return (
    <TouchableOpacity 
      style={styles.card} 
      activeOpacity={onPress ? 0.8 : 1} 
      onPress={onPress}
    >
      <View style={styles.topRow}>
        <View style={styles.titleContainer}>
          <View style={[styles.iconContainer, { backgroundColor: accentColor }]}>
            {icon}
          </View>
          <Text style={styles.title}>{title}</Text>
        </View>
        <View style={styles.chevronContainer}>
          <ChevronRight size={16} color={COLORS.secondary} />
        </View>
      </View>
      
      <View style={styles.contentContainer}>
        <Text style={styles.value}>{value}</Text>
        <Text style={styles.detail}>{detail}</Text>
      </View>

      <View style={[styles.bottomLine, { backgroundColor: accentColor }]} />
    </TouchableOpacity>
  );
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.radius,
    padding: SIZES.large,
    marginBottom: SIZES.medium,
    width: '48%', // Approx half width for 2-column grid
    position: 'relative',
    overflow: 'hidden',
  },
  topRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: SIZES.small,
  },
  titleContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: SIZES.small,
  },
  iconContainer: {
    width: 32,
    height: 32,
    borderRadius: 16,
    justifyContent: 'center',
    alignItems: 'center',
  },
  title: {
    color: COLORS.textMain,
    fontSize: SIZES.font,
    fontWeight: '500',
  },
  chevronContainer: {
    width: 24,
    height: 24,
    borderRadius: 12,
    backgroundColor: COLORS.surfaceLight,
    justifyContent: 'center',
    alignItems: 'center',
  },
  contentContainer: {
    marginTop: SIZES.base,
  },
  value: {
    color: COLORS.textMain,
    fontSize: SIZES.extraLarge + 2,
    fontWeight: 'bold',
    marginBottom: 4,
  },
  detail: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  bottomLine: {
    position: 'absolute',
    bottom: 0,
    left: SIZES.large,
    right: SIZES.large,
    height: 3,
    borderTopLeftRadius: 3,
    borderTopRightRadius: 3,
  }
});
