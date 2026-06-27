import React from 'react';
import { View, StyleSheet, ViewStyle } from 'react-native';
import { BlurView } from 'expo-blur';
import { COLORS, SIZES } from '../constants/theme';

interface GlassCardProps {
  children: React.ReactNode;
  style?: ViewStyle;
  intensity?: number;
}

export default function GlassCard({ children, style, intensity = 40 }: GlassCardProps) {
  return (
    <View style={[styles.container, style]}>
      <BlurView intensity={intensity} tint="dark" style={styles.blurView}>
        <View style={styles.content}>
          {children}
        </View>
      </BlurView>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    borderRadius: SIZES.cardRadius,
    overflow: 'hidden',
    borderColor: COLORS.glassBorder,
    borderWidth: 1,
    backgroundColor: COLORS.glassBackground, // fallback for older androids
  },
  blurView: {
    flex: 1,
  },
  content: {
    padding: SIZES.large,
    flex: 1,
  },
});
