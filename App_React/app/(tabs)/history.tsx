import React, { useMemo, useState } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, ActivityIndicator, Modal, TouchableWithoutFeedback } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { ChevronLeft, Filter, Globe, Cpu, LineChart, Clock, Download, Copy, ArrowDown } from 'lucide-react-native';
import { useRouter } from 'expo-router';
import { useQuery } from '@tanstack/react-query';
import * as FileSystem from 'expo-file-system/legacy';
import * as Sharing from 'expo-sharing';
import { COLORS, SIZES } from '../../src/constants/theme';
import { getHistoryMeta, getHistorySensors, getHistorySeries } from '../../src/services/modules/history';
import SelectorCard from '../../src/components/SelectorCard';
import { useHistoryFilterStore } from '../../src/store/useHistoryFilterStore';

// Helper to format date explicitly like the mockup "17/07/2025\n09:41:23"
const formatTableTime = (timestamp: string | number) => {
  const d = new Date(timestamp);
  if (isNaN(d.getTime())) return { date: '--/--/----', time: '--:--:--' };
  
  const pad = (n: number) => n.toString().padStart(2, '0');
  const date = `${pad(d.getDate())}/${pad(d.getMonth() + 1)}/${d.getFullYear()}`;
  const time = `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
  return { date, time };
};

export default function HistoryScreen() {
  const router = useRouter();

  // State
  const { selectedIp, selectedSensor, setSelectedIp, setSelectedSensor } = useHistoryFilterStore();
  const [currentPage, setCurrentPage] = useState(1);
  const [modalVisible, setModalVisible] = useState(false);
  const [modalType, setModalType] = useState<'ip' | 'sensor' | null>(null);

  const openModal = (type: 'ip' | 'sensor') => {
    setModalType(type);
    setModalVisible(true);
  };

  const rowsPerPage = 8;

  // Queries
  const ipsQuery = useQuery({ queryKey: ['history-meta'], queryFn: () => getHistoryMeta(100) });
  
  // Auto-select first IP if available and none selected
  const ips = ipsQuery.data || [];
  if (ips.length > 0 && !selectedIp) {
    setSelectedIp(ips[0].ip);
  }

  const sensorsQuery = useQuery({
    queryKey: ['history-sensors', selectedIp],
    queryFn: () => getHistorySensors(selectedIp),
    enabled: Boolean(selectedIp),
  });

  // Auto-select first sensor if available and none selected
  const sensors = sensorsQuery.data || [];
  if (sensors.length > 0 && !selectedSensor) {
    setSelectedSensor(sensors[0]);
  }

  const seriesQuery = useQuery({
    queryKey: ['history-series', selectedIp, selectedSensor],
    queryFn: () => getHistorySeries({ ip: selectedIp, sensor: selectedSensor, limit: 500 }),
    enabled: Boolean(selectedIp && selectedSensor),
  });

  // Data Transformation
  const items = seriesQuery.data || [];
  const tableRows = useMemo(() => {
    const map = new Map<string, any>();
    
    // Fallback Mock Data if API is empty just to show the UI
    if (items.length === 0 && !seriesQuery.isLoading) {
      const now = Date.now();
      return Array.from({ length: 50 }).map((_, i) => ({
        time: now - i * 60000,
        temp: (24 + Math.random() * 2).toFixed(1),
        hum: (55 + Math.random() * 5).toFixed(1),
        press: (1012 + Math.random() * 2).toFixed(1),
        alt: Math.floor(140 + Math.random() * 10),
      }));
    }

    items.forEach((item) => {
      const t = item.nodeTime || item.time;
      const key = String(t);
      if (!map.has(key)) {
        map.set(key, { time: t, temp: '--', hum: '--', press: '--', alt: '--' });
      }
      const row = map.get(key);
      const val = Number(item.value);
      const fieldLower = item.field.toLowerCase();
      
      if (fieldLower.includes('temp')) row.temp = val.toFixed(1);
      else if (fieldLower.includes('hum')) row.hum = val.toFixed(1);
      else if (fieldLower.includes('press')) row.press = val.toFixed(1);
      else if (fieldLower.includes('alt') || fieldLower.includes('elev')) row.alt = Math.round(val);
    });

    return Array.from(map.values()).sort((a, b) => {
      return new Date(b.time).getTime() - new Date(a.time).getTime();
    });
  }, [items, seriesQuery.isLoading]);

  // Pagination Logic
  const totalPages = Math.ceil(tableRows.length / rowsPerPage);
  const startIndex = (currentPage - 1) * rowsPerPage;
  const currentRows = tableRows.slice(startIndex, startIndex + rowsPerPage);
  const endDisplay = Math.min(startIndex + rowsPerPage, tableRows.length);

  // Export Logic
  const handleExport = async () => {
    if (tableRows.length === 0) return;
    
    let csvString = 'Date Time,Temperature (°C),Humidity (%),Pressure (hPa),Altitude (m)\n';
    tableRows.forEach(row => {
      const { date, time } = formatTableTime(row.time);
      csvString += `${date} ${time},${row.temp},${row.hum},${row.press},${row.alt}\n`;
    });

    try {
      const fileName = `Sensor_Data_${selectedIp || 'All'}_${Date.now()}.csv`;
      const fileUri = `${FileSystem.documentDirectory}${fileName}`;
      
      await FileSystem.writeAsStringAsync(fileUri, csvString);
      
      const isAvailable = await Sharing.isAvailableAsync();
      if (isAvailable) {
        await Sharing.shareAsync(fileUri, {
          mimeType: 'text/csv',
          dialogTitle: 'Export Sensor Data',
        });
      } else {
        alert('Sharing is not available on this device');
      }
    } catch (e) {
      console.error('Export error:', e);
      alert('Failed to export data');
    }
  };

  // Column Colors
  const COL_COLORS = {
    temp: '#a875ff', // Purple
    hum: COLORS.info, // Blue
    press: COLORS.success, // Green
    alt: '#ff288d', // Magenta
  };

  return (
    <SafeAreaView style={styles.container} edges={['top']}>
      {/* Header */}
      <View style={styles.header}>
        <View style={styles.headerTitleContainer}>
          <Text style={styles.headerTitle}>History</Text>
          <Text style={styles.headerSubtitle}>View past sensor data</Text>
        </View>
        <TouchableOpacity style={styles.filterButtonIcon}>
          <Filter size={20} color={COLORS.secondary} />
        </TouchableOpacity>
      </View>

      <ScrollView contentContainerStyle={styles.scrollContent} showsVerticalScrollIndicator={false}>
        
        {/* Selectors */}
        <SelectorCard
          title="Node IP"
          value={selectedIp || (ipsQuery.isLoading ? 'Loading...' : 'Select Node')}
          icon={<Globe size={20} color={COLORS.info} />}
          iconColor={COLORS.info}
          rightAction={selectedIp ? <Copy size={14} color={COLORS.info} /> : null}
          onPress={() => openModal('ip')}
        />
        
        <SelectorCard
          title="Sensor"
          value={selectedSensor || (sensorsQuery.isLoading ? 'Loading...' : 'Select Sensor')}
          icon={<Cpu size={20} color="#a875ff" />}
          iconColor="#a875ff"
          onPress={() => openModal('sensor')}
        />

        <SelectorCard
          title="Series"
          value="All Data"
          icon={<LineChart size={20} color={COLORS.success} />}
          iconColor={COLORS.success}
          onPress={() => {}}
        />

        {/* Data Records Section */}
        <View style={styles.recordsContainer}>
          <View style={styles.recordsHeader}>
            <View style={styles.recordsHeaderLeft}>
              <View style={styles.recordsIconContainer}>
                <Clock size={20} color={COLORS.info} />
              </View>
              <View>
                <Text style={styles.recordsTitle}>Data Records</Text>
                <Text style={styles.recordsSubtitle}>Latest {tableRows.length} records</Text>
              </View>
            </View>
            <TouchableOpacity style={styles.exportButton} onPress={handleExport}>
              <Download size={14} color={COLORS.secondary} />
              <Text style={styles.exportText}>Export</Text>
            </TouchableOpacity>
          </View>

          {/* Table */}
          {seriesQuery.isLoading ? (
            <View style={styles.loadingContainer}>
              <ActivityIndicator size="large" color={COLORS.primary} />
              <Text style={styles.loadingText}>Loading history data...</Text>
            </View>
          ) : (
            <View style={styles.table}>
              {/* Table Header */}
              <View style={styles.tableHeaderRow}>
                <View style={[styles.colHeader, { flex: 1.5 }]}>
                  <Text style={styles.colHeaderText}>Time</Text>
                  <ArrowDown size={12} color={COLORS.info} />
                </View>
                <View style={[styles.colHeader, { flex: 1 }]}>
                  <Text style={[styles.colHeaderText, { color: COL_COLORS.temp, textAlign: 'center' }]}>Temperature</Text>
                  <Text style={[styles.colHeaderUnit, { color: COL_COLORS.temp, textAlign: 'center' }]}>(°C)</Text>
                </View>
                <View style={[styles.colHeader, { flex: 1 }]}>
                  <Text style={[styles.colHeaderText, { color: COL_COLORS.hum, textAlign: 'center' }]}>Humidity</Text>
                  <Text style={[styles.colHeaderUnit, { color: COL_COLORS.hum, textAlign: 'center' }]}>(%)</Text>
                </View>
                <View style={[styles.colHeader, { flex: 1 }]}>
                  <Text style={[styles.colHeaderText, { color: COL_COLORS.press, textAlign: 'center' }]}>Pressure</Text>
                  <Text style={[styles.colHeaderUnit, { color: COL_COLORS.press, textAlign: 'center' }]}>(hPa)</Text>
                </View>
                <View style={[styles.colHeader, { flex: 0.8 }]}>
                  <Text style={[styles.colHeaderText, { color: COL_COLORS.alt, textAlign: 'right' }]}>Altitude</Text>
                  <Text style={[styles.colHeaderUnit, { color: COL_COLORS.alt, textAlign: 'right' }]}>(m)</Text>
                </View>
              </View>

              {/* Table Rows */}
              {currentRows.map((row, index) => {
                const { date, time } = formatTableTime(row.time);
                return (
                  <View key={index} style={[styles.tableRow, index % 2 === 1 && styles.tableRowAlternate]}>
                    <View style={[styles.colData, { flex: 1.5 }]}>
                      <Text style={styles.dateText}>{date}</Text>
                      <Text style={styles.timeText}>{time}</Text>
                    </View>
                    <View style={[styles.colData, { flex: 1 }]}>
                      <Text style={[styles.valText, { color: COL_COLORS.temp, textAlign: 'center' }]}>{row.temp}</Text>
                    </View>
                    <View style={[styles.colData, { flex: 1 }]}>
                      <Text style={[styles.valText, { color: COL_COLORS.hum, textAlign: 'center' }]}>{row.hum}</Text>
                    </View>
                    <View style={[styles.colData, { flex: 1 }]}>
                      <Text style={[styles.valText, { color: COL_COLORS.press, textAlign: 'center' }]}>{row.press}</Text>
                    </View>
                    <View style={[styles.colData, { flex: 0.8 }]}>
                      <Text style={[styles.valText, { color: COL_COLORS.alt, textAlign: 'right' }]}>{row.alt}</Text>
                    </View>
                  </View>
                );
              })}

              {/* Pagination */}
              <View style={styles.paginationRow}>
                <Text style={styles.paginationInfo}>
                  Showing {tableRows.length > 0 ? startIndex + 1 : 0} - {endDisplay} of {tableRows.length}
                </Text>
                
                <View style={styles.paginationControls}>
                  <TouchableOpacity 
                    style={styles.pageButton}
                    disabled={currentPage === 1}
                    onPress={() => setCurrentPage(prev => Math.max(1, prev - 1))}
                  >
                    <ChevronLeft size={16} color={currentPage === 1 ? COLORS.border : COLORS.secondary} />
                  </TouchableOpacity>
                  
                  {/* Simplistic page numbers for mockup feel */}
                  <View style={[styles.pageNum, styles.pageNumActive]}>
                    <Text style={styles.pageNumTextActive}>{currentPage}</Text>
                  </View>
                  {currentPage + 1 <= totalPages && (
                    <TouchableOpacity style={styles.pageNum} onPress={() => setCurrentPage(currentPage + 1)}>
                      <Text style={styles.pageNumText}>{currentPage + 1}</Text>
                    </TouchableOpacity>
                  )}
                  {currentPage + 2 <= totalPages && (
                    <TouchableOpacity style={styles.pageNum} onPress={() => setCurrentPage(currentPage + 2)}>
                      <Text style={styles.pageNumText}>{currentPage + 2}</Text>
                    </TouchableOpacity>
                  )}
                  
                  {totalPages > 3 && currentPage + 2 < totalPages && (
                    <>
                      <Text style={styles.pageDots}>...</Text>
                      <TouchableOpacity style={styles.pageNum} onPress={() => setCurrentPage(totalPages)}>
                        <Text style={styles.pageNumText}>{totalPages}</Text>
                      </TouchableOpacity>
                    </>
                  )}
                  
                  <TouchableOpacity 
                    style={styles.pageButton}
                    disabled={currentPage === totalPages || totalPages === 0}
                    onPress={() => setCurrentPage(prev => Math.min(totalPages, prev + 1))}
                  >
                    <ChevronLeft size={16} color={currentPage === totalPages ? COLORS.border : COLORS.secondary} style={{ transform: [{ rotate: '180deg' }] }} />
                  </TouchableOpacity>
                </View>
              </View>
            </View>
          )}
        </View>

      </ScrollView>

      {/* Selection Modal */}
      <Modal visible={modalVisible} transparent animationType="fade">
        <TouchableWithoutFeedback onPress={() => setModalVisible(false)}>
          <View style={styles.modalOverlay}>
            <TouchableWithoutFeedback>
              <View style={styles.modalContent}>
                <Text style={styles.modalTitle}>
                  Select {modalType === 'ip' ? 'Node IP' : 'Sensor'}
                </Text>
                
                {modalType === 'ip' && ips.map((ipRow: any) => (
                  <TouchableOpacity 
                    key={ipRow.ip} 
                    style={styles.modalOption}
                    onPress={() => {
                      if (selectedIp !== ipRow.ip) {
                        setSelectedIp(ipRow.ip);
                        setSelectedSensor(''); // reset sensor when IP changes
                      }
                      setModalVisible(false);
                    }}
                  >
                    <Text style={[styles.modalOptionText, selectedIp === ipRow.ip && styles.modalOptionSelected]}>
                      {ipRow.ip}
                    </Text>
                  </TouchableOpacity>
                ))}

                {modalType === 'sensor' && sensors.map((sensorName: string) => (
                  <TouchableOpacity 
                    key={sensorName} 
                    style={styles.modalOption}
                    onPress={() => {
                      setSelectedSensor(sensorName);
                      setModalVisible(false);
                    }}
                  >
                    <Text style={[styles.modalOptionText, selectedSensor === sensorName && styles.modalOptionSelected]}>
                      {sensorName}
                    </Text>
                  </TouchableOpacity>
                ))}
              </View>
            </TouchableWithoutFeedback>
          </View>
        </TouchableWithoutFeedback>
      </Modal>

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
    borderWidth: 1,
    borderColor: COLORS.border,
  },
  scrollContent: {
    padding: SIZES.large,
    paddingBottom: 40,
  },
  recordsContainer: {
    backgroundColor: COLORS.surface,
    borderRadius: SIZES.cardRadius,
    borderWidth: 1,
    borderColor: COLORS.border,
    overflow: 'hidden',
    marginTop: SIZES.small,
  },
  recordsHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: SIZES.large,
    borderBottomWidth: 1,
    borderBottomColor: COLORS.border,
  },
  recordsHeaderLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: SIZES.medium,
  },
  recordsIconContainer: {
    width: 36,
    height: 36,
    borderRadius: 18,
    backgroundColor: 'rgba(56, 189, 248, 0.15)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  recordsTitle: {
    color: COLORS.textMain,
    fontSize: SIZES.medium,
    fontWeight: 'bold',
    marginBottom: 2,
  },
  recordsSubtitle: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  exportButton: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    backgroundColor: COLORS.surfaceLight,
    paddingHorizontal: 12,
    paddingVertical: 8,
    borderRadius: SIZES.base,
    borderWidth: 1,
    borderColor: COLORS.border,
  },
  exportText: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
    fontWeight: '500',
  },
  table: {
    flex: 1,
  },
  tableHeaderRow: {
    flexDirection: 'row',
    paddingHorizontal: SIZES.large,
    paddingVertical: SIZES.medium,
    borderBottomWidth: 1,
    borderBottomColor: COLORS.border,
  },
  colHeader: {
    justifyContent: 'center',
  },
  colHeaderText: {
    color: COLORS.secondary,
    fontSize: 11,
    fontWeight: '500',
    marginBottom: 2,
  },
  colHeaderUnit: {
    fontSize: 10,
  },
  tableRow: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: SIZES.large,
    paddingVertical: 12,
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(255,255,255,0.03)',
  },
  tableRowAlternate: {
    backgroundColor: 'rgba(255,255,255,0.015)',
  },
  colData: {
    justifyContent: 'center',
  },
  dateText: {
    color: COLORS.secondary,
    fontSize: 11,
    marginBottom: 2,
  },
  timeText: {
    color: COLORS.secondary,
    fontSize: 11,
  },
  valText: {
    fontSize: 13,
    fontWeight: '600',
  },
  paginationRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: SIZES.large,
    paddingVertical: SIZES.medium,
    borderTopWidth: 1,
    borderTopColor: COLORS.border,
  },
  paginationInfo: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  paginationControls: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
  },
  pageButton: {
    width: 32,
    height: 32,
    borderRadius: 16,
    backgroundColor: COLORS.surfaceLight,
    justifyContent: 'center',
    alignItems: 'center',
  },
  pageNum: {
    width: 32,
    height: 32,
    borderRadius: 16,
    justifyContent: 'center',
    alignItems: 'center',
  },
  pageNumActive: {
    backgroundColor: COLORS.primary,
  },
  pageNumText: {
    color: COLORS.secondary,
    fontSize: SIZES.small,
  },
  pageNumTextActive: {
    color: '#fff',
    fontSize: SIZES.small,
    fontWeight: 'bold',
  },
  pageDots: {
    color: COLORS.secondary,
    marginHorizontal: 4,
  },
  loadingContainer: {
    padding: 40,
    alignItems: 'center',
  },
  loadingText: {
    color: COLORS.secondary,
    marginTop: 10,
  },
  modalOverlay: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.6)',
    justifyContent: 'center',
    alignItems: 'center',
    padding: SIZES.large,
  },
  modalContent: {
    backgroundColor: COLORS.surface,
    width: '100%',
    borderRadius: SIZES.cardRadius,
    padding: SIZES.large,
    borderWidth: 1,
    borderColor: COLORS.border,
  },
  modalTitle: {
    color: COLORS.textMain,
    fontSize: SIZES.large,
    fontWeight: 'bold',
    marginBottom: SIZES.medium,
    textAlign: 'center',
  },
  modalOption: {
    paddingVertical: SIZES.medium,
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(255,255,255,0.05)',
    alignItems: 'center',
  },
  modalOptionText: {
    color: COLORS.secondary,
    fontSize: SIZES.medium,
  },
  modalOptionSelected: {
    color: COLORS.primary,
    fontWeight: 'bold',
  }
});
