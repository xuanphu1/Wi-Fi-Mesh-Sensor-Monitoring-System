import React, { useState } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, TextInput } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { Menu, Filter, Search, ArrowUpDown, Activity, Plus, Thermometer, Droplet, Gauge, Mountain, Wind } from 'lucide-react-native';
import { LinearGradient } from 'expo-linear-gradient';
import { COLORS, SIZES } from '../../src/constants/theme';
import { useMeshStore } from '../../src/store/useMeshStore';
import SensorCard, { MetricData } from '../../src/components/SensorCard';

// Helper to generate nice random curves for the sparklines
const generateCurve = (base: number, variance: number, length: number) => {
  return Array.from({ length }, () => Math.max(0, Math.min(100, base + (Math.random() * variance * 2 - variance))));
};

export default function SensorsScreen() {
  const nodesRecord = useMeshStore((s) => s.nodes);
  const allNodes = Object.values(nodesRecord);
  
  const [searchQuery, setSearchQuery] = useState('');
  const [showMockData, setShowMockData] = useState(false); // Toggle to show mock data for demonstration

  // Check if we have real sensor data
  const hasRealData = allNodes.some(n => n.sensorEntries && n.sensorEntries.length > 0);
  const isPopulated = hasRealData || showMockData;

  // Render Empty State
  const renderEmptyState = () => (
    <View style={styles.emptyContainer}>
      <View style={styles.emptyGraphicContainer}>
        {/* Simple CSS-based window graphic */}
        <View style={styles.emptyBrowser}>
          <View style={styles.browserHeader}>
            <View style={styles.dot} />
            <View style={styles.dot} />
            <View style={styles.dot} />
          </View>
          <View style={styles.browserBody}>
            {/* CSS Chart line */}
            <View style={styles.cssChartLine} />
            <View style={styles.cssChartLine2} />
            <View style={styles.cssChartLine3} />
          </View>
          <View style={styles.magnifyingGlass}>
            <Search size={40} color="#8a2be2" />
          </View>
        </View>
      </View>
      
      <Text style={styles.emptyTitle}>No sensor data yet</Text>
      <Text style={styles.emptySubtitle}>
        Connect sensors to start monitoring{'\n'}and see live data here.
      </Text>
      
      <TouchableOpacity 
        style={styles.addButton}
        onPress={() => setShowMockData(true)} // Clicking this shows the mock UI!
      >
        <Plus size={20} color="#fff" />
        <Text style={styles.addButtonText}>Add Sensor</Text>
      </TouchableOpacity>
    </View>
  );

  // Define mock sensors matching the mockup
  const MOCK_SENSORS = [
    {
      sensorName: 'BME280 Sensor',
      ip: '192.168.1.101',
      location: 'Living Room',
      lastUpdate: '2s ago',
      status: 'Online' as const,
      metrics: [
        { title: 'Temperature', value: '24.6', unit: '°C', icon: <Thermometer size={14} color="#a875ff" />, colorHex: '#a875ff', mockPoints: generateCurve(60, 20, 15) },
        { title: 'Humidity', value: '56.2', unit: '%', icon: <Droplet size={14} color={COLORS.info} />, colorHex: COLORS.info, mockPoints: generateCurve(50, 10, 15) },
        { title: 'Pressure', value: '1013.2', unit: 'hPa', icon: <Gauge size={14} color={COLORS.success} />, colorHex: COLORS.success, mockPoints: generateCurve(80, 5, 15) },
        { title: 'Altitude', value: '145', unit: 'm', icon: <Mountain size={14} color="#a875ff" />, colorHex: '#a875ff', mockPoints: generateCurve(30, 5, 15) },
      ]
    },
    {
      sensorName: 'AHT20 Sensor',
      ip: '192.168.1.102',
      location: 'Bedroom',
      lastUpdate: '3s ago',
      status: 'Online' as const,
      metrics: [
        { title: 'Temperature', value: '25.1', unit: '°C', icon: <Thermometer size={14} color="#a875ff" />, colorHex: '#a875ff', mockPoints: generateCurve(65, 15, 15) },
        { title: 'Humidity', value: '58.7', unit: '%', icon: <Droplet size={14} color={COLORS.info} />, colorHex: COLORS.info, mockPoints: generateCurve(55, 15, 15) },
      ]
    },
    {
      sensorName: 'DHT22 Sensor',
      ip: '192.168.1.103',
      location: 'Outdoor',
      lastUpdate: '5s ago',
      status: 'Online' as const,
      metrics: [
        { title: 'Temperature', value: '23.8', unit: '°C', icon: <Thermometer size={14} color="#a875ff" />, colorHex: '#a875ff', mockPoints: generateCurve(50, 25, 15) },
        { title: 'Humidity', value: '60.3', unit: '%', icon: <Droplet size={14} color={COLORS.info} />, colorHex: COLORS.info, mockPoints: generateCurve(65, 20, 15) },
      ]
    },
    {
      sensorName: 'BME680 Sensor',
      ip: '192.168.1.104',
      location: 'Office',
      lastUpdate: '8s ago',
      status: 'Warning' as const,
      metrics: [
        { title: 'Temperature', value: '26.3', unit: '°C', icon: <Thermometer size={14} color="#a875ff" />, colorHex: '#a875ff', mockPoints: generateCurve(75, 10, 15) },
        { title: 'Humidity', value: '55.1', unit: '%', icon: <Droplet size={14} color={COLORS.info} />, colorHex: COLORS.info, mockPoints: generateCurve(50, 15, 15) },
        { title: 'Pressure', value: '1012.6', unit: 'hPa', icon: <Gauge size={14} color={COLORS.success} />, colorHex: COLORS.success, mockPoints: generateCurve(78, 5, 15) },
        { title: 'Gas', value: '156', unit: 'Ω', icon: <Wind size={14} color={COLORS.warning} />, colorHex: COLORS.warning, mockPoints: generateCurve(40, 30, 15) },
      ]
    }
  ];

  // Map real data if we have it, otherwise use mocks if toggled
  let displayData: any[] = [];
  if (hasRealData) {
    allNodes.filter(n => n.sensorEntries?.length > 0).forEach(n => {
      // Group entries by sensorName (e.g. BME280, AHT10)
      const grouped: Record<string, any[]> = {};
      n.sensorEntries.forEach((entry: any) => {
        const name = entry.sensorName || 'Unknown Sensor';
        if (!grouped[name]) grouped[name] = [];
        grouped[name].push(entry);
      });

      Object.entries(grouped).forEach(([sensorName, entries]) => {
        // Map real node sensor entries to MetricData
        const metrics: MetricData[] = entries.map((entry: any) => {
          let title = 'Value';
          let unit = '';
          let icon = <Activity size={14} color={COLORS.primary} />;
          let colorHex = COLORS.primary;
          
          if (entry.key === 'temp_c') { title = 'Temperature'; unit = '°C'; icon = <Thermometer size={14} color="#a875ff"/>; colorHex = '#a875ff'; }
          else if (entry.key === 'hum_rh') { title = 'Humidity'; unit = '%'; icon = <Droplet size={14} color={COLORS.info}/>; colorHex = COLORS.info; }
          else if (entry.key === 'press_hpa') { title = 'Pressure'; unit = 'hPa'; icon = <Gauge size={14} color={COLORS.success}/>; colorHex = COLORS.success; }
          else if (entry.key === 'gas_res_ohm') { title = 'Gas'; unit = 'Ω'; icon = <Wind size={14} color={COLORS.warning}/>; colorHex = COLORS.warning; }
          
          return {
            title: entry.label || title,
            value: entry.value !== undefined ? String(entry.value) : '0',
            unit: entry.unit || unit,
            icon,
            colorHex,
            mockPoints: generateCurve(50, 20, 15) // We don't have history in store, so we must mock the sparkline
          };
        });

        displayData.push({
          sensorName: sensorName,
          ip: n.ip,
          location: n.name || 'Unknown Node',
          lastUpdate: 'Just now',
          status: n.status as any,
          metrics
        });
      });
    });
  } else if (showMockData) {
    displayData = MOCK_SENSORS;
  }

  // Apply Search Filter
  let filteredData = displayData;
  if (searchQuery.trim() !== '') {
    const lowerQuery = searchQuery.toLowerCase();
    filteredData = displayData.filter((s) => 
      s.sensorName.toLowerCase().includes(lowerQuery) ||
      s.ip.toLowerCase().includes(lowerQuery) ||
      s.location.toLowerCase().includes(lowerQuery)
    );
  }

  return (
    <SafeAreaView style={styles.container} edges={['top']}>
      {/* Header */}
      <View style={styles.header}>
        <View style={styles.headerTitleContainer}>
          <Text style={styles.headerTitle}>Sensors</Text>
          <Text style={styles.headerSubtitle}>Realtime sensor {isPopulated ? 'data' : 'monitoring'}</Text>
        </View>
        <TouchableOpacity style={styles.filterButtonIcon}>
          <Filter size={20} color={COLORS.secondary} />
        </TouchableOpacity>
      </View>

      <ScrollView contentContainerStyle={styles.scrollContent} showsVerticalScrollIndicator={false}>
        
        {/* Hero Banner (Only if Empty) */}
        {!isPopulated && (
          <LinearGradient
            colors={['#101633', '#151e45']}
            style={styles.heroCard}
            start={{ x: 0, y: 0 }}
            end={{ x: 1, y: 1 }}
          >
            <View style={styles.heroContent}>
              <View style={styles.heroTextContent}>
                <View style={styles.heroTitleRow}>
                  <View style={styles.heroIconContainer}>
                    <Activity size={20} color={COLORS.info} />
                  </View>
                  <Text style={styles.heroTitleText}>Realtime Sensors</Text>
                </View>
                <Text style={styles.heroSubtitleText}>
                  Live sensor data charts and metrics will appear here.
                </Text>
              </View>
              
              {/* 3D Isometric Placeholder */}
              <View style={styles.isoPlaceholder}>
                <View style={[styles.isoBlock, { bottom: 10, left: 20 }]} />
                <View style={[styles.isoBlock, { bottom: 25, left: 40 }]} />
                <View style={[styles.isoBlock, { bottom: -5, left: 50 }]} />
                <View style={styles.isoGlow} />
              </View>
            </View>
          </LinearGradient>
        )}

        {/* Populated Content */}
        {isPopulated ? (
          <>
            {/* Search and Sort */}
            <View style={styles.searchSection}>
              <View style={styles.searchContainer}>
                <Search size={20} color={COLORS.secondary} style={styles.searchIcon} />
                <TextInput
                  style={styles.searchInput}
                  placeholder="Search sensors or IP..."
                  placeholderTextColor={COLORS.secondary}
                  value={searchQuery}
                  onChangeText={setSearchQuery}
                />
              </View>
              <TouchableOpacity style={styles.sortButton}>
                <ArrowUpDown size={16} color={COLORS.secondary} />
                <Text style={styles.sortText}>Sort</Text>
              </TouchableOpacity>
            </View>

            {/* Sensor List */}
            <View style={styles.listContainer}>
              {filteredData.map((sensor, idx) => (
                <SensorCard
                  key={idx}
                  sensorName={sensor.sensorName}
                  ip={sensor.ip}
                  location={sensor.location}
                  lastUpdate={sensor.lastUpdate}
                  status={sensor.status}
                  metrics={sensor.metrics}
                />
              ))}
            </View>
          </>
        ) : (
          renderEmptyState()
        )}

      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: SIZES.large,
    paddingVertical: SIZES.medium,
  },
  headerButton: {
    width: 40,
    height: 40,
    justifyContent: 'center',
    alignItems: 'flex-start',
  },
  headerTitleContainer: {
    flex: 1,
  },
  headerTitle: {
    fontSize: SIZES.extraLarge,
    fontWeight: 'bold',
    color: COLORS.textMain,
  },
  headerSubtitle: {
    fontSize: SIZES.small,
    color: COLORS.secondary,
  },
  filterButtonIcon: {
    width: 40,
    height: 40,
    borderRadius: 20,
    backgroundColor: COLORS.surfaceLight,
    justifyContent: 'center',
    alignItems: 'center',
  },
  scrollContent: {
    padding: SIZES.large,
    paddingBottom: 40,
  },
  
  // Hero Section
  heroCard: {
    borderRadius: SIZES.cardRadius,
    padding: SIZES.large,
    marginBottom: SIZES.xl,
    borderWidth: 1,
    borderColor: COLORS.border,
    overflow: 'hidden',
  },
  heroContent: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
  },
  heroTextContent: {
    flex: 1,
    paddingRight: SIZES.medium,
  },
  heroTitleRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: SIZES.small,
    marginBottom: SIZES.small,
  },
  heroIconContainer: {
    width: 36,
    height: 36,
    borderRadius: 8,
    backgroundColor: 'rgba(56, 189, 248, 0.2)', // Info blue tint
    justifyContent: 'center',
    alignItems: 'center',
  },
  heroTitleText: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: 'bold',
  },
  heroSubtitleText: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
    lineHeight: 18,
  },
  isoPlaceholder: {
    width: 100,
    height: 80,
    position: 'relative',
  },
  isoBlock: {
    position: 'absolute',
    width: 30,
    height: 10,
    backgroundColor: '#1b4a99',
    borderTopWidth: 2,
    borderTopColor: '#38bdf8',
    borderLeftWidth: 2,
    borderLeftColor: '#2888c7',
    transform: [{ rotateX: '60deg' }, { rotateZ: '45deg' }],
    zIndex: 2,
  },
  isoGlow: {
    position: 'absolute',
    top: 20,
    left: 20,
    width: 60,
    height: 40,
    borderRadius: 30,
    backgroundColor: COLORS.info,
    opacity: 0.3,
    zIndex: 1,
  },

  // Search Section
  searchSection: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: SIZES.small,
    marginBottom: SIZES.large,
  },
  searchContainer: {
    flex: 1,
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: COLORS.surface,
    borderWidth: 1,
    borderColor: COLORS.border,
    borderRadius: SIZES.radius,
    paddingHorizontal: SIZES.medium,
    height: 48,
  },
  searchIcon: {
    marginRight: SIZES.small,
  },
  searchInput: {
    flex: 1,
    color: COLORS.textMain,
    fontSize: SIZES.font,
    height: '100%',
  },
  sortButton: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    height: 48,
    paddingHorizontal: SIZES.medium,
    backgroundColor: COLORS.surface,
    borderWidth: 1,
    borderColor: COLORS.border,
    borderRadius: SIZES.radius,
  },
  sortText: {
    color: COLORS.secondary,
    fontSize: SIZES.font,
  },

  // List
  listContainer: {
    paddingBottom: SIZES.xl,
  },

  // Empty State
  emptyContainer: {
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: 40,
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.cardRadius,
    borderWidth: 1,
    borderColor: COLORS.border,
    borderStyle: 'dashed',
  },
  emptyGraphicContainer: {
    marginBottom: SIZES.xl,
    position: 'relative',
  },
  emptyBrowser: {
    width: 160,
    height: 120,
    backgroundColor: 'rgba(25, 33, 65, 0.8)',
    borderRadius: SIZES.radius,
    borderWidth: 2,
    borderColor: 'rgba(56, 189, 248, 0.3)',
    overflow: 'hidden',
  },
  browserHeader: {
    height: 24,
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(56, 189, 248, 0.2)',
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 10,
    gap: 4,
  },
  dot: {
    width: 6,
    height: 6,
    borderRadius: 3,
    backgroundColor: 'rgba(255, 255, 255, 0.2)',
  },
  browserBody: {
    flex: 1,
    padding: 15,
    justifyContent: 'center',
    gap: 15,
  },
  cssChartLine: {
    height: 2,
    backgroundColor: 'rgba(56, 189, 248, 0.4)',
    width: '100%',
    transform: [{ rotate: '-15deg' }],
  },
  cssChartLine2: {
    height: 2,
    backgroundColor: 'rgba(56, 189, 248, 0.6)',
    width: '60%',
    transform: [{ rotate: '25deg' }, { translateX: 20 }, { translateY: -10 }],
  },
  cssChartLine3: {
    height: 2,
    backgroundColor: 'rgba(138, 43, 226, 0.6)',
    width: '80%',
    transform: [{ rotate: '-5deg' }, { translateX: 10 }, { translateY: -5 }],
  },
  magnifyingGlass: {
    position: 'absolute',
    bottom: -15,
    right: -15,
    backgroundColor: COLORS.background,
    borderRadius: 30,
    padding: 4,
  },
  emptyTitle: {
    fontSize: SIZES.large,
    fontWeight: 'bold',
    color: COLORS.textMain,
    marginBottom: SIZES.small,
  },
  emptySubtitle: {
    fontSize: SIZES.font,
    color: COLORS.secondary,
    textAlign: 'center',
    lineHeight: 22,
    marginBottom: SIZES.xl,
  },
  addButton: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: COLORS.primary,
    paddingHorizontal: SIZES.xl,
    paddingVertical: 12,
    borderRadius: SIZES.base,
    gap: 8,
  },
  addButtonText: {
    color: '#fff',
    fontWeight: 'bold',
    fontSize: SIZES.font,
  }
});
