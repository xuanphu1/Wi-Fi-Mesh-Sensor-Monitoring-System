import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity } from 'react-native';
import { ChevronRight } from 'lucide-react-native';
import { COLORS, SIZES } from '../constants/theme';

interface SettingMenuCardProps {
  title: string;
  subtitle: string;
  icon: React.ReactNode;
  iconColor: string;
  onPress: () => void;
}

export default function SettingMenuCard({ title, subtitle, icon, iconColor, onPress }: SettingMenuCardProps) {
  return (
    <TouchableOpacity 
      style={styles.card} 
      activeOpacity={0.7}
      onPress={onPress}
    >
      <View style={styles.content}>
        <View style={[styles.iconContainer, { backgroundColor: `${iconColor}20` }]}>
          {icon}
        </View>
        
        <View style={styles.textContainer}>
          <Text style={styles.title}>{title}</Text>
          <Text style={styles.subtitle}>{subtitle}</Text>
        </View>

        <ChevronRight size={20} color={COLORS.secondary} />
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
  textContainer: {
    flex: 1,
    justifyContent: 'center',
    paddingRight: SIZES.medium,
  },
  title: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: 'bold',
    marginBottom: 4,
  },
  subtitle: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
    lineHeight: 18,
  }
});
