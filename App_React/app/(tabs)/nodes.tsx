import React, { useState } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, TextInput } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { ChevronLeft, Plus, Server, Search, Filter } from 'lucide-react-native';
import { useRouter } from 'expo-router';
import { LinearGradient } from 'expo-linear-gradient';
import { COLORS, SIZES } from '../../src/constants/theme';
import { useMeshStore } from '../../src/store/useMeshStore';
import NodeCard from '../../src/components/NodeCard';

export default function NodesScreen() {
  const router = useRouter();
  const nodesRecord = useMeshStore((s) => s.nodes);
  const allNodes = Object.values(nodesRecord);
  const [searchQuery, setSearchQuery] = useState('');

  // Fallback mock arrays for visuals as per plan
  const MOCK_BOARD_TYPES = ['ESP32-S3', 'ESP32', 'ESP8266', 'ESP32', 'ESP32-S2'];
  const MOCK_RSSI = ['-45 dBm', '-52 dBm', '-61 dBm', '-48 dBm', '-67 dBm'];
  const CHIP_COLORS = [COLORS.success, COLORS.primary, COLORS.warning, COLORS.info, '#ff288d'];

  // If there are no nodes in the store yet, we provide some dummy ones so the UI looks exactly like the mockup.
  const displayNodes = allNodes.length > 0 ? allNodes : [
    { ip: '192.168.1.101', name: 'ESP Living Room', status: 'Online' as const },
    { ip: '192.168.1.102', name: 'ESP Bedroom', status: 'Online' as const },
    { ip: '192.168.1.103', name: 'ESP Kitchen', status: 'Online' as const },
    { ip: '192.168.1.104', name: 'ESP Garage', status: 'Online' as const },
    { ip: '192.168.1.105', name: 'ESP Outdoor', status: 'Online' as const },
  ];

  return (
    <SafeAreaView style={styles.container} edges={['top']}>
      {/* Header */}
      <View style={styles.header}>
        <View style={styles.headerTitleContainer}>
          <Text style={styles.headerTitle}>Nodes</Text>
          <Text style={styles.headerSubtitle}>Manage your mesh network nodes</Text>
        </View>
      </View>

      <ScrollView contentContainerStyle={styles.scrollContent} showsVerticalScrollIndicator={false}>
        
        {/* Search Bar */}
        <View style={styles.searchSection}>
          <View style={styles.searchContainer}>
            <Search size={20} color={COLORS.secondary} style={styles.searchIcon} />
            <TextInput
              style={styles.searchInput}
              placeholder="Search nodes..."
              placeholderTextColor={COLORS.secondary}
              value={searchQuery}
              onChangeText={setSearchQuery}
            />
          </View>
          <TouchableOpacity style={styles.filterButton}>
            <Filter size={20} color={COLORS.secondary} />
          </TouchableOpacity>
        </View>

        {/* Section Title */}
        <View style={styles.sectionHeader}>
          <Text style={styles.sectionTitle}>Connected Nodes ({displayNodes.length})</Text>
          <View style={styles.onlineIndicator}>
            <Text style={styles.onlineText}>Online</Text>
            <View style={styles.onlineDot} />
          </View>
        </View>

        {/* Nodes List */}
        <View style={styles.listContainer}>
          {displayNodes.map((node, index) => (
            <NodeCard
              key={node.ip}
              name={node.name}
              ip={node.ip}
              status={node.status}
              boardType={MOCK_BOARD_TYPES[index % MOCK_BOARD_TYPES.length]}
              rssi={MOCK_RSSI[index % MOCK_RSSI.length]}
              colorHex={CHIP_COLORS[index % CHIP_COLORS.length]}
              isMain={index === 0}
            />
          ))}
        </View>

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
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: SIZES.large,
    paddingVertical: SIZES.medium,
  },
  headerTitleContainer: {
    flex: 1,
  },
  headerSubtitle: {
    fontSize: SIZES.small,
    color: COLORS.secondary,
  },
  headerTitle: {
    fontSize: SIZES.extraLarge,
    fontWeight: 'bold',
    color: COLORS.textMain,
  },
  scrollContent: {
    padding: SIZES.large,
    paddingBottom: 40,
  },
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
    width: 32,
    height: 32,
    borderRadius: 8,
    backgroundColor: 'rgba(138, 43, 226, 0.2)', // Purple tint
    justifyContent: 'center',
    alignItems: 'center',
  },
  heroTitle: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: 'bold',
  },
  heroSubtitle: {
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
    width: 24,
    height: 18,
    backgroundColor: '#3b2575',
    borderTopWidth: 2,
    borderTopColor: '#5e40b3',
    borderLeftWidth: 2,
    borderLeftColor: '#452b8c',
    transform: [{ rotateX: '60deg' }, { rotateZ: '45deg' }],
    zIndex: 2,
  },
  isoGlow: {
    position: 'absolute',
    top: 10,
    left: 10,
    width: 80,
    height: 60,
    borderRadius: 40,
    backgroundColor: '#8a2be2',
    opacity: 0.15,
    zIndex: 1,
  },
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
  filterButton: {
    width: 48,
    height: 48,
    backgroundColor: COLORS.surface,
    borderWidth: 1,
    borderColor: COLORS.border,
    borderRadius: SIZES.radius,
    justifyContent: 'center',
    alignItems: 'center',
  },
  sectionHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: SIZES.medium,
    paddingHorizontal: 4,
  },
  sectionTitle: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: 'bold',
  },
  onlineIndicator: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
  },
  onlineText: {
    color: COLORS.success,
    fontSize: SIZES.small,
    fontWeight: '500',
  },
  onlineDot: {
    width: 8,
    height: 8,
    borderRadius: 4,
    backgroundColor: COLORS.success,
    shadowColor: COLORS.success,
    shadowOffset: { width: 0, height: 0 },
    shadowOpacity: 0.8,
    shadowRadius: 4,
  },
  listContainer: {
    paddingBottom: SIZES.xl,
  }
});
