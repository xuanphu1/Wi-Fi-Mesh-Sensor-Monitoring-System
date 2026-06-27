import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { COLORS, SIZES } from '../../src/constants/theme';

export default function PlaceholderScreen() {
  return (
    <SafeAreaView style={styles.container}>
      <Text style={styles.text}>Ports & Sensors Screen Under Construction...</Text>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
    justifyContent: 'center',
    alignItems: 'center',
  },
  text: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
  }
});
