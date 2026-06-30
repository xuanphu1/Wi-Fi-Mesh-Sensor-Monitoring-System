import React, { useState } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, TextInput } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { ChevronLeft, Plus, Search, Server, Cpu, Droplet, ChevronRight } from 'lucide-react-native';
import { useRouter } from 'expo-router';
import { COLORS, SIZES } from '../../src/constants/theme';
import { useMeshStore } from '../../src/store/useMeshStore';
import { getSensorTypeByName, getFieldsForSensorType } from '../../src/constants/mesh';

const getSensorFieldsText = (sensorName: string) => {
  const type = getSensorTypeByName(sensorName);
  const fields = getFieldsForSensorType(type);
  if (fields.length === 0) return 'No fields available';
  return fields.map(f => f.label).join(', ');
};

// Helper icon
const getSensorIcon = (name: string) => {
  if (name.includes('AHT') || name.includes('DHT')) return <Droplet size={16} color={COLORS.info} />;
  if (name.includes('BME680')) return <Cpu size={16} color={COLORS.warning} />;
  return <Cpu size={16} color="#a875ff" />;
};

const getSensorIconBg = (name: string) => {
  if (name.includes('AHT') || name.includes('DHT')) return 'rgba(56, 189, 248, 0.15)';
  if (name.includes('BME680')) return 'rgba(251, 146, 60, 0.15)';
  return 'rgba(168, 117, 255, 0.15)';
};

export default function SensorThresholdsScreen() {
  const router = useRouter();
  const [searchQuery, setSearchQuery] = useState('');
  
  const nodesRecord = useMeshStore((s) => s.nodes);
  const allNodes = Object.values(nodesRecord);

  // Filter nodes by search query
  const displayNodes = allNodes.filter(n => 
    n.ip.includes(searchQuery) || 
    (n.name && n.name.toLowerCase().includes(searchQuery.toLowerCase()))
  );

  return (
    <SafeAreaView style={styles.container} edges={['top']}>
      {/* Header */}
      <View style={styles.header}>
        <TouchableOpacity style={styles.headerButton} onPress={() => router.back()}>
          <ChevronLeft size={24} color={COLORS.textMain} />
        </TouchableOpacity>
        <View style={styles.headerTitleContainer}>
          <Text style={styles.headerTitle}>Sensor Thresholds</Text>
          <Text style={styles.headerSubtitle}>Set limits for sensor fields</Text>
        </View>
        <TouchableOpacity style={styles.addButtonIcon}>
          <Plus size={20} color={COLORS.info} />
        </TouchableOpacity>
      </View>

      {/* Search Bar */}
      <View style={styles.searchSection}>
        <View style={styles.searchContainer}>
          <Search size={20} color={COLORS.secondary} style={styles.searchIcon} />
          <TextInput
            style={styles.searchInput}
            placeholder="Search nodes or sensors..."
            placeholderTextColor={COLORS.secondary}
            value={searchQuery}
            onChangeText={setSearchQuery}
          />
        </View>
      </View>

      <ScrollView contentContainerStyle={styles.scrollContent} showsVerticalScrollIndicator={false}>
        {displayNodes.length === 0 ? (
          <View style={styles.emptyContainer}>
            <Text style={styles.emptyText}>No nodes or sensors found.</Text>
          </View>
        ) : (
          displayNodes.map((node, index) => {
            const isOnline = node.status === 'Online';
            
            // Extract unique sensor names from the node's sensorEntries
            const uniqueSensors = Array.from(new Set(node.sensorEntries?.map(s => s.sensorName) || []));
            
            return (
              <View key={node.ip} style={styles.nodeCard}>
                {/* Node Header Row */}
                <View style={styles.nodeHeaderRow}>
                  <View style={styles.nodeHeaderLeft}>
                    <View style={styles.nodeIconContainer}>
                      <Server size={18} color={COLORS.secondary} />
                    </View>
                    <View>
                      <Text style={styles.nodeIp}>{node.ip}</Text>
                      <Text style={styles.nodeName}>{node.name || 'Unknown Node'}</Text>
                    </View>
                  </View>
                  <Text style={[styles.nodeStatus, { color: isOnline ? COLORS.success : COLORS.secondary }]}>
                    {node.status}
                  </Text>
                </View>

                {/* Nested Sensors */}
                {uniqueSensors.length > 0 ? (
                  <View style={styles.sensorsList}>
                    {uniqueSensors.flatMap((sensorName) => {
                      const fields = getFieldsForSensorType(getSensorTypeByName(sensorName));
                      return fields.map((field) => (
                        <TouchableOpacity 
                          key={`${sensorName}-${field.key}`}
                          style={styles.sensorItemRow}
                          activeOpacity={0.7}
                          onPress={() => {
                            router.push({
                              pathname: '/settings/edit-threshold',
                              params: { ip: node.ip, sensor: sensorName, field: field.label, fieldKey: field.key, unit: field.unit }
                            });
                          }}
                        >
                          <View style={[styles.sensorIconContainer, { backgroundColor: getSensorIconBg(sensorName) }]}>
                            {getSensorIcon(sensorName)}
                          </View>
                          <View style={styles.sensorTextContainer}>
                            <Text style={styles.sensorTitle}>{sensorName}</Text>
                            <Text style={styles.sensorFields}>Configure {field.label}</Text>
                          </View>
                          <ChevronRight size={18} color={COLORS.secondary} />
                        </TouchableOpacity>
                      ));
                    })}
                  </View>
                ) : (
                  <View style={styles.noSensorsContainer}>
                    <Text style={styles.noSensorsText}>No sensors detected on this node.</Text>
                  </View>
                )}
              </View>
            );
          })
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
    fontSize: SIZES.large,
    fontWeight: 'bold',
    color: COLORS.textMain,
    marginBottom: 2,
  },
  headerSubtitle: {
    fontSize: SIZES.small,
    color: COLORS.secondary,
  },
  addButtonIcon: {
    width: 36,
    height: 36,
    borderRadius: 18,
    backgroundColor: 'rgba(56, 189, 248, 0.1)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  
  searchSection: {
    paddingHorizontal: SIZES.large,
    marginBottom: SIZES.medium,
  },
  searchContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: COLORS.surface,
    borderWidth: 1,
    borderColor: COLORS.border,
    borderRadius: SIZES.radius,
    paddingHorizontal: SIZES.medium,
    height: 44,
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
  
  scrollContent: {
    padding: SIZES.large,
    paddingBottom: 40,
  },
  
  nodeCard: {
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.cardRadius,
    marginBottom: SIZES.medium,
    borderWidth: 1,
    borderColor: COLORS.border,
    overflow: 'hidden',
  },
  nodeHeaderRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: SIZES.medium,
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(255,255,255,0.03)',
  },
  nodeHeaderLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: SIZES.medium,
  },
  nodeIconContainer: {
    width: 36,
    height: 36,
    borderRadius: 8,
    backgroundColor: COLORS.surfaceLight,
    justifyContent: 'center',
    alignItems: 'center',
  },
  nodeIp: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: '600',
    marginBottom: 2,
  },
  nodeName: {
    color: COLORS.secondary,
    fontSize: 11,
  },
  nodeStatus: {
    fontSize: 11,
    fontWeight: '600',
  },
  
  sensorsList: {
    paddingVertical: SIZES.small,
  },
  sensorItemRow: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: SIZES.small,
    paddingHorizontal: SIZES.medium,
  },
  sensorIconContainer: {
    width: 32,
    height: 32,
    borderRadius: 8,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: SIZES.medium,
  },
  sensorTextContainer: {
    flex: 1,
    paddingRight: SIZES.small,
  },
  sensorTitle: {
    color: COLORS.textMain,
    fontSize: SIZES.font,
    fontWeight: '500',
    marginBottom: 2,
  },
  sensorFields: {
    color: COLORS.secondary,
    fontSize: 11,
  },
  
  noSensorsContainer: {
    padding: SIZES.large,
    alignItems: 'center',
  },
  noSensorsText: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  emptyContainer: {
    padding: 40,
    alignItems: 'center',
  },
  emptyText: {
    color: COLORS.secondary,
  }
});
