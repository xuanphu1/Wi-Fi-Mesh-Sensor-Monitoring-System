import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
import Svg, { Path, Defs, LinearGradient, Stop } from 'react-native-svg';
import { COLORS, SIZES } from '../constants/theme';

interface MiniMetricCardProps {
  icon: React.ReactNode;
  title: string;
  value: string | number;
  unit: string;
  colorHex: string;
  mockPoints: number[]; // Array of 0-100 values to draw the sparkline
}

export default function MiniMetricCard({ icon, title, value, unit, colorHex, mockPoints }: MiniMetricCardProps) {
  
  // Convert mockPoints (0-100) into SVG path coordinates
  // Card width is approx 150px, height is 30px for the chart area
  const CHART_WIDTH = 150;
  const CHART_HEIGHT = 25;
  
  const generatePath = () => {
    if (!mockPoints || mockPoints.length === 0) return '';
    const stepX = CHART_WIDTH / (mockPoints.length - 1);
    
    const path = mockPoints.reduce((acc, point, index) => {
      // Invert Y because SVG 0,0 is top-left
      const y = CHART_HEIGHT - (point / 100) * CHART_HEIGHT;
      const x = index * stepX;
      
      if (index === 0) return `M ${x} ${y}`;
      
      // Simple bezier curve smoothing
      const prevX = (index - 1) * stepX;
      const prevY = CHART_HEIGHT - (mockPoints[index - 1] / 100) * CHART_HEIGHT;
      
      const cp1X = prevX + stepX / 2;
      const cp1Y = prevY;
      const cp2X = x - stepX / 2;
      const cp2Y = y;
      
      return `${acc} C ${cp1X} ${cp1Y}, ${cp2X} ${cp2Y}, ${x} ${y}`;
    }, '');
    
    return path;
  };

  const d = generatePath();

  return (
    <View style={styles.card}>
      <View style={styles.header}>
        {icon}
        <Text style={styles.title}>{title}</Text>
      </View>
      
      <View style={styles.valueRow}>
        <Text style={styles.value}>{value}</Text>
        <Text style={styles.unit}>{unit}</Text>
      </View>

      <View style={styles.chartContainer}>
        <Svg width="100%" height={CHART_HEIGHT} viewBox={`0 0 ${CHART_WIDTH} ${CHART_HEIGHT}`} preserveAspectRatio="none">
          <Defs>
            <LinearGradient id="grad" x1="0" y1="0" x2="1" y2="0">
              <Stop offset="0" stopColor={colorHex} stopOpacity="0.4" />
              <Stop offset="0.5" stopColor={colorHex} stopOpacity="1" />
              <Stop offset="1" stopColor={colorHex} stopOpacity="0.4" />
            </LinearGradient>
          </Defs>
          <Path
            d={d}
            stroke="url(#grad)"
            strokeWidth={2}
            fill="none"
            strokeLinecap="round"
            strokeLinejoin="round"
          />
        </Svg>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: 'rgba(25, 33, 65, 0.5)',
    borderRadius: SIZES.radius,
    padding: SIZES.medium,
    width: '48%', // Allows 2 cards per row with gap
    marginBottom: SIZES.medium,
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.05)',
    overflow: 'hidden',
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    marginBottom: SIZES.small,
  },
  title: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  valueRow: {
    flexDirection: 'row',
    alignItems: 'baseline',
    gap: 4,
    marginBottom: SIZES.small,
  },
  value: {
    color: COLORS.textMain,
    fontSize: SIZES.large,
    fontWeight: 'bold',
  },
  unit: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  chartContainer: {
    width: '100%',
    height: 25,
    marginTop: 4,
    marginLeft: -4, // Shift left to align perfectly with padding
  }
});
