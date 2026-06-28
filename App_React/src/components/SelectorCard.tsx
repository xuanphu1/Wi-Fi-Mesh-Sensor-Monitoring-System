import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity } from 'react-native';
import { ChevronRight } from 'lucide-react-native';
import { COLORS, SIZES } from '../constants/theme';

interface SelectorCardProps {
  title: string;
  value: string;
  icon: React.ReactNode;
  iconColor: string;
  onPress?: () => void;
  rightAction?: React.ReactNode;
}

export default function SelectorCard({ title, value, icon, iconColor, onPress, rightAction }: SelectorCardProps) {
  return (
    <TouchableOpacity 
      style={styles.card} 
      activeOpacity={onPress ? 0.7 : 1}
      onPress={onPress}
    >
      <View style={styles.content}>
        <View style={[styles.iconContainer, { backgroundColor: `${iconColor}20` }]}>
          {icon}
        </View>
        
        <View style={styles.textContainer}>
          <Text style={styles.title} numberOfLines={1}>{title}</Text>
          <View style={styles.valueRow}>
            <Text style={styles.value} numberOfLines={1}>{value}</Text>
            {rightAction && <View style={styles.rightAction}>{rightAction}</View>}
          </View>
        </View>

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
    padding: SIZES.medium,
    borderWidth: 1,
    borderColor: COLORS.border,
    height: 72,
    justifyContent: 'center',
  },
  content: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  iconContainer: {
    width: 44,
    height: 44,
    borderRadius: 12,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: SIZES.medium,
  },
  textContainer: {
    flex: 1,
    justifyContent: 'center',
  },
  title: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
    marginBottom: 4,
  },
  valueRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  value: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: 'bold',
  },
  rightAction: {
    justifyContent: 'center',
    alignItems: 'center',
  },
  chevronContainer: {
    justifyContent: 'center',
    alignItems: 'center',
    paddingLeft: SIZES.small,
  }
});
