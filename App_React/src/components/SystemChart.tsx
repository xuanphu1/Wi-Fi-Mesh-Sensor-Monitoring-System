import React from 'react';
import { View, Text, StyleSheet, Dimensions } from 'react-native';
import { LineChart } from 'react-native-gifted-charts';
import { TrendingUp, ChevronDown } from 'lucide-react-native';
import { COLORS, SIZES } from '../constants/theme';

import { useMeshStore } from '../store/useMeshStore';

export default function SystemChart() {
  const throughputSeries = useMeshStore(s => s.throughputSeries);
  const chartData = throughputSeries.length > 0 ? throughputSeries : [{value: 0}];

  const screenWidth = Dimensions.get('window').width;
  // Subtracting the padding of the screen (SIZES.large * 2), padding of the chart container, and space for Y-axis text
  const chartWidth = screenWidth - (SIZES.large * 2) - 60; // Increased margin a bit more
  const chartSpacing = chartWidth / (chartData.length - 1);

  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <View style={styles.titleRow}>
          <TrendingUp size={24} color={COLORS.primary} />
          <View style={styles.titleTexts}>
            <Text style={styles.title}>Server Throughput</Text>
            <Text style={styles.subtitle}>Bytes per second received from WebSocket</Text>
          </View>
        </View>
      </View>

      <View style={styles.chartContainer}>
        <LineChart
          data={chartData}
          width={chartWidth}
          height={180}
          thickness={3}
          color={COLORS.primary}
          hideDataPoints
          dataPointsColor={COLORS.primary}
          startFillColor={COLORS.primary}
          endFillColor={COLORS.primary}
          startOpacity={0.4}
          endOpacity={0.0}
          yAxisColor={COLORS.border}
          xAxisColor={COLORS.border}
          yAxisTextStyle={{ color: COLORS.secondary, fontSize: 10 }}
          xAxisLabelTextStyle={{ color: COLORS.secondary, fontSize: 10 }}
          rulesColor={COLORS.border}
          rulesType="dashed"
          yAxisLabelTexts={['0', '100', '200', '300', '400']}
          maxValue={400}
          noOfSections={4}
          stepValue={100}
          areaChart
          curved
          isAnimated
          animationDuration={1200}
          showFractionalValues={false}
          hideYAxisText={false}
          initialSpacing={0}
          spacing={chartSpacing}
          yAxisLabelWidth={40}
        />
        
        <View style={styles.xLabelsRow}>
          <Text style={styles.xLabel}>08:40</Text>
          <Text style={styles.xLabel}>08:50</Text>
          <Text style={styles.xLabel}>09:00</Text>
          <Text style={styles.xLabel}>09:10</Text>
          <Text style={styles.xLabel}>09:20</Text>
          <Text style={styles.xLabel}>09:30</Text>
          <Text style={styles.xLabel}>09:40</Text>
        </View>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.radius,
    padding: SIZES.large,
    marginBottom: SIZES.large,
    overflow: 'hidden',
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'flex-start',
    marginBottom: SIZES.xl,
  },
  titleRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: SIZES.small,
    flex: 1, // Allow this to shrink so it doesn't push the dropdown out
    paddingRight: SIZES.small, // Add some padding so it doesn't touch the dropdown
  },
  titleTexts: {
    flexDirection: 'column',
    flexShrink: 1,
  },
  title: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: 'bold',
    marginBottom: 2,
  },
  subtitle: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  chartContainer: {
    marginLeft: -10, // Adjust for the Y-axis label space
  },
  xLabelsRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingLeft: 35, // Align with the start of the chart
    paddingRight: 10,
    marginTop: 8,
  },
  xLabel: {
    color: COLORS.secondary,
    fontSize: 10,
  }
});
