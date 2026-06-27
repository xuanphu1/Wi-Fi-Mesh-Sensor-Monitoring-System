import React from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, ActivityIndicator } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { Menu, RefreshCw, Router, Cpu, CheckCircle2, ChevronRight, ChevronDown } from 'lucide-react-native';
import { useQuery } from '@tanstack/react-query';
import { COLORS, SIZES } from '../../src/constants/theme';
import { listFirmware } from '../../src/services/modules/firmware';
import { useMeshStore } from '../../src/store/useMeshStore';
import FirmwareCard from '../../src/components/FirmwareCard';

export default function FirmwareScreen() {
  const gateway = useMeshStore((s) => s.gateway);
  const nodesByIp = useMeshStore((s) => s.nodes);
  const nodes = Object.values(nodesByIp);
  
  const firmwareQuery = useQuery({ queryKey: ['firmware'], queryFn: listFirmware });

  const variants = nodes.reduce<Record<string, number>>((acc, node) => {
    acc[node.firmwareVersion] = (acc[node.firmwareVersion] || 0) + 1;
    return acc;
  }, {});

  const observedVariantsCount = Object.keys(variants).length;
  const currentGatewayVersion = gateway?.firmware_version || '1.2.3'; // Fallback to mockup value if empty

  // Mock server firmware data matching the mockup exactly
  const MOCK_SERVER_FIRMWARE = [
    { title: 'ESP32 Firmware', version: 'v2.1.0', releaseTime: 'Released 3 days ago', size: '2.4 MB', iconColor: COLORS.success, isLatest: true },
    { title: 'ESP8266 Firmware', version: 'v1.4.7', releaseTime: 'Released 7 days ago', size: '1.8 MB', iconColor: COLORS.info, isLatest: false },
    { title: 'STM32 Firmware', version: 'v3.0.2', releaseTime: 'Released 12 days ago', size: '3.1 MB', iconColor: COLORS.warning, isLatest: false },
    { title: 'NRF52 Firmware', version: 'v1.9.5', releaseTime: 'Released 20 days ago', size: '2.0 MB', iconColor: '#ff288d', isLatest: false }, // Magenta
    { title: 'RP2040 Firmware', version: 'v1.2.8', releaseTime: 'Released 1 month ago', size: '1.6 MB', iconColor: '#a875ff', isLatest: false }, // Purple
  ];

  // Use real data if available and has items, otherwise use mock to showcase UI
  const realDataAvailable = firmwareQuery.data && firmwareQuery.data.length > 0;
  
  let displayData = [];
  if (realDataAvailable) {
    displayData = firmwareQuery.data.map((item, index) => {
      // Rotate colors based on index
      const colorsList = [COLORS.success, COLORS.info, COLORS.warning, '#ff288d', '#a875ff'];
      const iconColor = colorsList[index % colorsList.length];
      
      return {
        title: `${item.object} Firmware`,
        version: item.version,
        releaseTime: new Date(item.uploadedAt).toLocaleDateString(),
        size: `${(item.size / 1024 / 1024).toFixed(1)} MB`,
        iconColor,
        isLatest: index === 0
      };
    });
  } else {
    displayData = MOCK_SERVER_FIRMWARE;
  }

  return (
    <SafeAreaView style={styles.container} edges={['top']}>
      {/* Header */}
      <View style={styles.header}>
        <View style={styles.headerTitleContainer}>
          <Text style={styles.headerTitle}>Firmware Update</Text>
          <Text style={styles.headerSubtitle}>OTA update for mesh nodes</Text>
        </View>
        <TouchableOpacity style={styles.refreshButtonIcon}>
          <RefreshCw size={20} color={COLORS.info} />
        </TouchableOpacity>
      </View>

      <ScrollView contentContainerStyle={styles.scrollContent} showsVerticalScrollIndicator={false}>
        
        {/* Status Cards */}
        <TouchableOpacity style={styles.statusCard} activeOpacity={0.7}>
          <View style={styles.statusCardContent}>
            <View style={[styles.statusIconContainer, { backgroundColor: 'rgba(56, 189, 248, 0.15)' }]}>
              <Router size={24} color={COLORS.info} />
            </View>
            <View style={styles.statusTextContainer}>
              <Text style={styles.statusTitle}>Gateway firmware</Text>
              <View style={styles.statusRow}>
                <Text style={styles.statusSubtitle}>Current version</Text>
                <View style={styles.versionBadgeInfo}>
                  <Text style={styles.versionTextInfo}>{currentGatewayVersion}</Text>
                </View>
              </View>
              <View style={styles.statusSuccessRow}>
                <CheckCircle2 size={14} color={COLORS.success} />
                <Text style={styles.statusSuccessText}>Up to date</Text>
              </View>
            </View>
            <ChevronRight size={20} color={COLORS.secondary} />
          </View>
        </TouchableOpacity>

        <TouchableOpacity style={styles.statusCard} activeOpacity={0.7}>
          <View style={styles.statusCardContent}>
            <View style={[styles.statusIconContainer, { backgroundColor: 'rgba(168, 117, 255, 0.15)' }]}>
              <Cpu size={24} color="#a875ff" />
            </View>
            <View style={styles.statusTextContainer}>
              <Text style={styles.statusTitle}>Node firmware variants</Text>
              <View style={styles.statusRow}>
                <Text style={styles.statusSubtitle}>Observed variants</Text>
                <View style={styles.versionBadgePurple}>
                  <Text style={styles.versionTextPurple}>{observedVariantsCount > 0 ? observedVariantsCount : 3}</Text>
                </View>
              </View>
              <View style={styles.statusSuccessRow}>
                <CheckCircle2 size={14} color={COLORS.success} />
                <Text style={styles.statusSuccessText}>All variants are up to date</Text>
              </View>
            </View>
            <ChevronRight size={20} color={COLORS.secondary} />
          </View>
        </TouchableOpacity>

        {/* Server Firmware List Section */}
        <View style={styles.listSection}>
          <View style={styles.listHeader}>
            <View>
              <Text style={styles.listTitle}>Server firmware list</Text>
              <Text style={styles.listSubtitle}>Available firmware on server</Text>
            </View>
            <TouchableOpacity 
              style={styles.refreshPill}
              onPress={() => firmwareQuery.refetch()}
            >
              <RefreshCw size={14} color={COLORS.info} />
              <Text style={styles.refreshPillText}>Refresh</Text>
            </TouchableOpacity>
          </View>

          {firmwareQuery.isLoading && !realDataAvailable ? (
            <ActivityIndicator size="large" color={COLORS.primary} style={{ marginVertical: 20 }} />
          ) : (
            <View style={styles.firmwareList}>
              {displayData.map((fw, idx) => (
                <FirmwareCard
                  key={idx}
                  title={fw.title}
                  version={fw.version}
                  releaseTime={fw.releaseTime}
                  size={fw.size}
                  iconColor={fw.iconColor}
                  isLatest={fw.isLatest}
                />
              ))}
            </View>
          )}
          
          <TouchableOpacity style={styles.viewMoreButton}>
            <Text style={styles.viewMoreText}>View more</Text>
            <ChevronDown size={16} color={COLORS.info} />
          </TouchableOpacity>
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
  refreshButtonIcon: {
    width: 40,
    height: 40,
    borderRadius: 20,
    backgroundColor: 'rgba(56, 189, 248, 0.1)',
    justifyContent: 'center',
    alignItems: 'center',
    borderWidth: 1,
    borderColor: 'rgba(56, 189, 248, 0.2)',
  },
  scrollContent: {
    padding: SIZES.large,
    paddingBottom: 40,
  },
  
  // Status Cards
  statusCard: {
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.cardRadius,
    marginBottom: SIZES.medium,
    padding: SIZES.large,
    borderWidth: 1,
    borderColor: COLORS.border,
  },
  statusCardContent: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  statusIconContainer: {
    width: 48,
    height: 48,
    borderRadius: 12,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: SIZES.medium,
  },
  statusTextContainer: {
    flex: 1,
  },
  statusTitle: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: 'bold',
    marginBottom: 6,
  },
  statusRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    marginBottom: 6,
  },
  statusSubtitle: {
    color: COLORS.secondary,
    fontSize: SIZES.font,
  },
  versionBadgeInfo: {
    backgroundColor: 'rgba(56, 189, 248, 0.15)',
    paddingHorizontal: 6,
    paddingVertical: 2,
    borderRadius: 4,
  },
  versionTextInfo: {
    color: COLORS.info,
    fontSize: 12,
    fontWeight: 'bold',
  },
  versionBadgePurple: {
    backgroundColor: 'rgba(168, 117, 255, 0.15)',
    paddingHorizontal: 6,
    paddingVertical: 2,
    borderRadius: 4,
    minWidth: 20,
    alignItems: 'center',
  },
  versionTextPurple: {
    color: '#a875ff',
    fontSize: 12,
    fontWeight: 'bold',
  },
  statusSuccessRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
  },
  statusSuccessText: {
    color: COLORS.success,
    fontSize: SIZES.small,
    fontWeight: '500',
  },

  // List Section
  listSection: {
    marginTop: SIZES.medium,
  },
  listHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: SIZES.large,
  },
  listTitle: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: 'bold',
    marginBottom: 4,
  },
  listSubtitle: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  refreshPill: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    backgroundColor: 'rgba(56, 189, 248, 0.1)',
    paddingHorizontal: 12,
    paddingVertical: 8,
    borderRadius: 20,
    borderWidth: 1,
    borderColor: 'rgba(56, 189, 248, 0.2)',
  },
  refreshPillText: {
    color: COLORS.textMain,
    fontSize: SIZES.small,
    fontWeight: '500',
  },
  firmwareList: {
    flex: 1,
  },
  viewMoreButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: SIZES.medium,
    gap: 4,
  },
  viewMoreText: {
    color: COLORS.info,
    fontSize: SIZES.font,
    fontWeight: '500',
  }
});
