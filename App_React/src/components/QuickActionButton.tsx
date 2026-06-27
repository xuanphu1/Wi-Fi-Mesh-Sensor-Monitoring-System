import React from 'react';
import { TouchableOpacity, View, Text, StyleSheet, ViewStyle } from 'react-native';
import { ChevronRight } from 'lucide-react-native';
import { COLORS, SIZES } from '../constants/theme';

interface QuickActionButtonProps {
  icon: React.ReactNode;
  title: string;
  subtitle: string;
  onPress: () => void;
  style?: ViewStyle;
}

export default function QuickActionButton({ icon, title, subtitle, onPress, style }: QuickActionButtonProps) {
  return (
    <TouchableOpacity style={[styles.container, style]} onPress={onPress} activeOpacity={0.7}>
      <View style={styles.iconContainer}>
        {icon}
      </View>
      <View style={styles.textContainer}>
        <Text style={styles.title}>{title}</Text>
        <Text style={styles.subtitle}>{subtitle}</Text>
      </View>
      <View style={styles.chevronContainer}>
        <ChevronRight size={16} color={COLORS.secondary} />
      </View>
    </TouchableOpacity>
  );
}

const styles = StyleSheet.create({
  container: {
    backgroundColor: COLORS.surfaceLight,
    borderRadius: SIZES.radius,
    padding: SIZES.medium,
    width: 140,
    marginRight: SIZES.small,
  },
  iconContainer: {
    width: 40,
    height: 40,
    borderRadius: 20,
    backgroundColor: 'rgba(255, 255, 255, 0.05)',
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: SIZES.small,
  },
  textContainer: {
    flex: 1,
  },
  title: {
    color: COLORS.textMain,
    fontSize: SIZES.font,
    fontWeight: 'bold',
    marginBottom: 4,
  },
  subtitle: {
    color: COLORS.secondary,
    fontSize: SIZES.small - 1,
  },
  chevronContainer: {
    position: 'absolute',
    top: SIZES.medium,
    right: SIZES.medium,
  }
});
