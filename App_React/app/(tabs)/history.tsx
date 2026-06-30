import React, { useMemo, useState, useEffect } from 'react';
import { View, Text, StyleSheet, ScrollView, TouchableOpacity, ActivityIndicator, Modal, TouchableWithoutFeedback, Dimensions, Platform, Alert } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { ChevronLeft, Filter, Globe, Cpu, LineChart, Clock, Download, ArrowDown, HardDrive, BarChart2 } from 'lucide-react-native';
import { useQuery } from '@tanstack/react-query';
import { LineChart as GiftedLineChart } from 'react-native-gifted-charts';
import { COLORS, SIZES } from '../../src/constants/theme';
import { getHistoryMeta, getHistorySeries } from '../../src/services/modules/history';
import { getSensorTypeByName, getFieldsForSensorType } from '../../src/constants/mesh';
import SelectorCard from '../../src/components/SelectorCard';
import DateTimePicker from '@react-native-community/datetimepicker';
import { useHistoryFilterStore } from '../../src/store/useHistoryFilterStore';
import * as FileSystem from 'expo-file-system/legacy';
import * as Sharing from 'expo-sharing';

const formatTableTime = (timestamp: string | number) => {
  const d = new Date(timestamp);
  if (isNaN(d.getTime())) return { date: '--/--/----', time: '--:--:--' };
  
  const pad = (n: number) => n.toString().padStart(2, '0');
  const date = `${pad(d.getDate())}/${pad(d.getMonth() + 1)}/${d.getFullYear()}`;
  const time = `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
  return { date, time };
};

const formatDuration = (ms: number) => {
  const h = Math.floor(ms / 3600000);
  const m = Math.floor((ms % 3600000) / 60000);
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m`;
};

export default function HistoryScreen() {
  const { 
    selectedMac, selectedSensorName, selectedField, timeRange, customStartTime,
    setSelectedMac, setSelectedSensorName, setSelectedField, setTimeRange, setCustomStartTime
  } = useHistoryFilterStore();

  const [modalVisible, setModalVisible] = useState(false);
  const [modalType, setModalType] = useState<'mac' | 'sensor' | 'field' | 'time' | null>(null);
  const [showDatePicker, setShowDatePicker] = useState(false);
  const [tempDate, setTempDate] = useState(new Date());

  // Queries
  const metaQuery = useQuery({ queryKey: ['history-meta'], queryFn: () => getHistoryMeta(200) });
  
  const macs = metaQuery.data || [];
  
  // Derived state
  const availableSensors = useMemo(() => {
    const selectedMacObj = macs.find(m => m.mac === selectedMac);
    return selectedMacObj?.sensors || [];
  }, [macs, selectedMac]);

  const availableFields = useMemo(() => {
    if (!selectedSensorName) return [];
    const typeId = getSensorTypeByName(selectedSensorName);
    return getFieldsForSensorType(typeId);
  }, [selectedSensorName]);

  // Auto-selection logic
  useEffect(() => {
    if (macs.length > 0 && !selectedMac) {
      setSelectedMac(macs[0].mac);
    }
  }, [macs, selectedMac]);

  useEffect(() => {
    if (availableSensors.length > 0 && (!selectedSensorName || !availableSensors.includes(selectedSensorName))) {
      setSelectedSensorName(availableSensors[0]);
    }
  }, [availableSensors, selectedSensorName]);

  useEffect(() => {
    if (availableFields.length > 0 && (!selectedField || !availableFields.find(f => f.key === selectedField))) {
      setSelectedField(availableFields[0].key);
    }
  }, [availableFields, selectedField]);

  // Fetch Series
  const { from, to } = useMemo(() => {
    const end = new Date();
    let start = new Date();
    if (timeRange === '1h') start.setHours(end.getHours() - 1);
    else if (timeRange === '24h') start.setDate(end.getDate() - 1);
    else if (timeRange === '7d') start.setDate(end.getDate() - 7);
    else if (timeRange === 'custom' && customStartTime) start = new Date(customStartTime);
    else start.setDate(end.getDate() - 1); // fallback
    return { from: start.toISOString(), to: end.toISOString() };
  }, [timeRange, customStartTime]);

  const seriesQuery = useQuery({
    queryKey: ['history-series', selectedMac, selectedSensorName, selectedField, from, to],
    queryFn: () => getHistorySeries({ mac: selectedMac, sensorName: selectedSensorName, field: selectedField, from, to, limit: 2000 }),
    enabled: Boolean(selectedMac && selectedSensorName && selectedField),
  });

  const items = seriesQuery.data || [];

  // Compute Statistics
  const stats = useMemo(() => {
    if (items.length === 0) return { min: 0, max: 0, avg: 0, total: 0, spanMs: 0 };
    
    let min = items[0].value;
    let max = items[0].value;
    let sum = 0;
    
    let minTime = new Date(items[0].time).getTime();
    let maxTime = minTime;

    items.forEach(it => {
      if (it.value < min) min = it.value;
      if (it.value > max) max = it.value;
      sum += it.value;
      
      const t = new Date(it.time).getTime();
      if (t < minTime) minTime = t;
      if (t > maxTime) maxTime = t;
    });

    return {
      min, max, avg: sum / items.length, total: items.length, spanMs: maxTime - minTime
    };
  }, [items]);

  const chartData = useMemo(() => {
    if (items.length === 0) return [{ value: 0 }];
    // Reduce points if there are too many to draw smoothly
    const step = Math.max(1, Math.floor(items.length / 100));
    const pts = [];
    for (let i = 0; i < items.length; i += step) {
      pts.push({ value: items[i].value });
    }
    return pts;
  }, [items]);

  const openModal = (type: 'mac' | 'sensor' | 'field' | 'time') => {
    setModalType(type);
    setModalVisible(true);
  };

  const activeFieldMeta = availableFields.find(f => f.key === selectedField);

  return (
    <SafeAreaView style={styles.container} edges={['top']}>
      {/* Header */}
      <View style={styles.header}>
        <View style={styles.headerTitleContainer}>
          <Text style={styles.headerTitle}>History / Charts</Text>
          <Text style={styles.headerSubtitle}>Analyze node data over time</Text>
        </View>
        <View style={{ flexDirection: 'row', alignItems: 'center', gap: 12 }}>
          {seriesQuery.isFetching && <ActivityIndicator size="small" color={COLORS.primary} />}
          <TouchableOpacity 
            style={styles.exportButton}
            onPress={async () => {
              if (selectedMac && items.length > 0) {
                try {
                  let csv = "Time,Value\n";
                  items.forEach(it => {
                    csv += `${it.time},${it.value}\n`;
                  });
                  
                  const fileUri = FileSystem.documentDirectory + `${selectedMac.replace(/:/g, '')}_${selectedField}.csv`;
                  await FileSystem.writeAsStringAsync(fileUri, csv, { encoding: 'utf8' });
                  
                  if (await Sharing.isAvailableAsync()) {
                    await Sharing.shareAsync(fileUri);
                  } else {
                    Alert.alert("Lỗi", "Không thể chia sẻ file trên thiết bị này");
                  }
                } catch (e) {
                  Alert.alert("Lỗi", "Đã xảy ra lỗi khi tạo file");
                  console.error(e);
                }
              } else {
                Alert.alert("Thông báo", "Không có dữ liệu để tải xuống");
              }
            }}
          >
            <Download size={20} color={COLORS.textMain} />
          </TouchableOpacity>
        </View>
      </View>

      <ScrollView contentContainerStyle={styles.scrollContent} showsVerticalScrollIndicator={false}>
        
        {/* Filters Grid */}
        <View style={styles.filterGrid}>
          <View style={styles.filterCol}>
            <SelectorCard
              title="Select MAC"
              value={selectedMac || 'Select...'}
              icon={<HardDrive size={20} color={COLORS.info} />}
              iconColor={COLORS.info}
              onPress={() => openModal('mac')}
            />
          </View>
          <View style={styles.filterCol}>
            <SelectorCard
              title="Select Sensor"
              value={selectedSensorName ? selectedSensorName.replace('SENSOR_', '') : 'Select...'}
              icon={<Cpu size={20} color={COLORS.warning} />}
              iconColor={COLORS.warning}
              onPress={() => openModal('sensor')}
            />
          </View>
          <View style={styles.filterCol}>
            <SelectorCard
              title="Select Field"
              value={activeFieldMeta ? `${activeFieldMeta.label} (${activeFieldMeta.unit})` : 'Select...'}
              icon={<BarChart2 size={20} color={COLORS.success} />}
              iconColor={COLORS.success}
              onPress={() => openModal('field')}
            />
          </View>
          <View style={styles.filterCol}>
            <SelectorCard
              title="Start Time"
              value={customStartTime ? new Date(customStartTime).toLocaleString() : 'Select Time...'}
              icon={<Clock size={20} color={COLORS.primary} />}
              iconColor={COLORS.primary}
              onPress={() => {
                if (Platform.OS === 'ios') {
                  setTempDate(customStartTime ? new Date(customStartTime) : new Date());
                  setModalType('time');
                  setModalVisible(true);
                } else {
                  setShowDatePicker(true);
                }
              }}
            />
          </View>
        </View>

        {/* Chart Section */}
        <View style={styles.chartCard}>
          <Text style={styles.chartMetaText}>
            MAC: {selectedMac} • Sensor: {selectedSensorName} • Field: {activeFieldMeta?.label} • Points: {stats.total}
          </Text>
          
          <View style={styles.chartContainer}>
            {items.length === 0 ? (
              <View style={styles.chartEmpty}>
                <Text style={styles.chartEmptyText}>No data available for this selection.</Text>
              </View>
            ) : (
              <GiftedLineChart
                data={chartData}
                width={Dimensions.get('window').width - (SIZES.large * 2) - 60}
                height={160}
                thickness={2}
                color={COLORS.info}
                hideDataPoints
                startFillColor={COLORS.info}
                endFillColor={COLORS.info}
                startOpacity={0.3}
                endOpacity={0.0}
                yAxisColor={COLORS.border}
                xAxisColor={COLORS.border}
                yAxisTextStyle={{ color: COLORS.secondary, fontSize: 10 }}
                rulesColor={COLORS.border}
                rulesType="dashed"
                areaChart
                curved
                isAnimated
                initialSpacing={0}
                spacing={(Dimensions.get('window').width - (SIZES.large * 2) - 60) / Math.max(1, chartData.length - 1)}
                yAxisLabelWidth={40}
              />
            )}
          </View>
        </View>

        {/* Stats Grid */}
        <View style={styles.statsGrid}>
          <View style={styles.statCard}>
            <View style={[styles.statIconBox, { backgroundColor: `${COLORS.primary}20` }]}>
              <BarChart2 size={16} color={COLORS.primary} />
            </View>
            <View>
              <Text style={styles.statLabel}>Average</Text>
              <Text style={styles.statValue}>{stats.avg.toFixed(2)} {activeFieldMeta?.unit}</Text>
            </View>
          </View>

          <View style={styles.statCard}>
            <View style={[styles.statIconBox, { backgroundColor: `${COLORS.info}20` }]}>
              <ArrowDown size={16} color={COLORS.info} />
            </View>
            <View>
              <Text style={styles.statLabel}>Min</Text>
              <Text style={styles.statValue}>{stats.min.toFixed(2)} {activeFieldMeta?.unit}</Text>
            </View>
          </View>

          <View style={styles.statCard}>
            <View style={[styles.statIconBox, { backgroundColor: `${COLORS.success}20` }]}>
              <LineChart size={16} color={COLORS.success} />
            </View>
            <View>
              <Text style={styles.statLabel}>Max</Text>
              <Text style={styles.statValue}>{stats.max.toFixed(2)} {activeFieldMeta?.unit}</Text>
            </View>
          </View>

          <View style={styles.statCard}>
            <View style={[styles.statIconBox, { backgroundColor: `${COLORS.warning}20` }]}>
              <Globe size={16} color={COLORS.warning} />
            </View>
            <View>
              <Text style={styles.statLabel}>Total Points</Text>
              <Text style={styles.statValue}>{stats.total}</Text>
            </View>
          </View>

          <View style={styles.statCard}>
            <View style={[styles.statIconBox, { backgroundColor: `${COLORS.error}20` }]}>
              <Clock size={16} color={COLORS.error} />
            </View>
            <View>
              <Text style={styles.statLabel}>Displayed Timespan</Text>
              <Text style={styles.statValue}>{formatDuration(stats.spanMs)}</Text>
            </View>
          </View>
        </View>

      </ScrollView>

      {/* Modal Dropdown */}
      <Modal visible={modalVisible} transparent animationType="fade">
        <TouchableWithoutFeedback onPress={() => setModalVisible(false)}>
          <View style={styles.modalOverlay}>
            <TouchableWithoutFeedback>
              <View style={styles.modalContent}>
                <Text style={styles.modalTitle}>
                  {modalType === 'mac' && 'Select MAC Address'}
                  {modalType === 'sensor' && 'Select Sensor'}
                  {modalType === 'field' && 'Select Field'}
                  {modalType === 'time' && 'Select Time Range'}
                </Text>

                <ScrollView style={styles.modalList} showsVerticalScrollIndicator={false}>
                  {modalType === 'mac' && macs.map((m) => (
                    <TouchableOpacity
                      key={m.mac}
                      style={[styles.modalItem, selectedMac === m.mac && styles.modalItemSelected]}
                      onPress={() => { setSelectedMac(m.mac); setModalVisible(false); }}
                    >
                      <Text style={[styles.modalItemText, selectedMac === m.mac && styles.modalItemTextSelected]}>
                        {m.mac}
                      </Text>
                    </TouchableOpacity>
                  ))}

                  {modalType === 'sensor' && availableSensors.map((s) => (
                    <TouchableOpacity
                      key={s}
                      style={[styles.modalItem, selectedSensorName === s && styles.modalItemSelected]}
                      onPress={() => { setSelectedSensorName(s); setModalVisible(false); }}
                    >
                      <Text style={[styles.modalItemText, selectedSensorName === s && styles.modalItemTextSelected]}>
                        {s.replace('SENSOR_', '')}
                      </Text>
                    </TouchableOpacity>
                  ))}

                  {modalType === 'field' && availableFields.map((f) => (
                    <TouchableOpacity
                      key={f.key}
                      style={[styles.modalItem, selectedField === f.key && styles.modalItemSelected]}
                      onPress={() => { setSelectedField(f.key); setModalVisible(false); }}
                    >
                      <Text style={[styles.modalItemText, selectedField === f.key && styles.modalItemTextSelected]}>
                        {f.label} ({f.unit})
                      </Text>
                    </TouchableOpacity>
                  ))}
                  {modalType === 'time' && Platform.OS === 'ios' && (
                    <View style={{ alignItems: 'center', paddingVertical: 20 }}>
                      <DateTimePicker
                        value={tempDate}
                        mode="datetime"
                        display="spinner"
                        themeVariant="dark"
                        textColor={COLORS.text}
                        onChange={(event, selectedDate) => {
                          if (selectedDate) setTempDate(selectedDate);
                        }}
                      />
                      <TouchableOpacity
                        style={{ marginTop: 20, backgroundColor: COLORS.primary, paddingHorizontal: 40, paddingVertical: 12, borderRadius: 8 }}
                        onPress={() => {
                          setCustomStartTime(tempDate.getTime());
                          setTimeRange('custom');
                          setModalVisible(false);
                        }}
                      >
                        <Text style={{ color: '#fff', fontSize: 16, fontWeight: 'bold' }}>Confirm Date & Time</Text>
                      </TouchableOpacity>
                    </View>
                  )}
                </ScrollView>
              </View>
            </TouchableWithoutFeedback>
          </View>
        </TouchableWithoutFeedback>
      </Modal>

      {/* Android DateTime Picker */}
      {showDatePicker && Platform.OS === 'android' && (
        <DateTimePicker
          value={customStartTime ? new Date(customStartTime) : new Date()}
          mode="datetime"
          is24Hour={true}
          display="default"
          onChange={(event, selectedDate) => {
            setShowDatePicker(false);
            if (event.type === 'set' && selectedDate) {
              setCustomStartTime(selectedDate.getTime());
              setTimeRange('custom');
            }
          }}
        />
      )}
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: COLORS.background },
  header: {
    flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center',
    paddingHorizontal: SIZES.large, paddingVertical: SIZES.medium,
    backgroundColor: COLORS.surface, borderBottomWidth: 1, borderBottomColor: COLORS.border,
  },
  headerTitleContainer: { flex: 1 },
  headerTitle: { fontSize: 22, fontWeight: 'bold', color: COLORS.textMain },
  headerSubtitle: { fontSize: 14, color: COLORS.secondary, marginTop: 2 },
  exportButton: { width: 40, height: 40, borderRadius: 12, backgroundColor: `${COLORS.info}20`, alignItems: 'center', justifyContent: 'center' },
  scrollContent: { padding: SIZES.large, paddingBottom: 100 },
  
  filterGrid: { flexDirection: 'row', flexWrap: 'wrap', justifyContent: 'space-between' },
  filterCol: { width: '48%', marginBottom: SIZES.medium },

  chartCard: {
    backgroundColor: COLORS.surface, borderRadius: SIZES.cardRadius,
    padding: SIZES.medium, borderWidth: 1, borderColor: COLORS.border,
    marginBottom: SIZES.large, overflow: 'hidden'
  },
  chartMetaText: { fontSize: 12, color: COLORS.secondary, marginBottom: 16 },
  chartContainer: { alignItems: 'center', justifyContent: 'center', minHeight: 160, overflow: 'hidden' },
  chartEmpty: { flex: 1, alignItems: 'center', justifyContent: 'center' },
  chartEmptyText: { color: COLORS.secondary, fontSize: 14 },

  statsGrid: { flexDirection: 'row', flexWrap: 'wrap', gap: SIZES.small },
  statCard: {
    backgroundColor: COLORS.surface, borderRadius: SIZES.radius,
    padding: SIZES.medium, borderWidth: 1, borderColor: COLORS.border,
    flexDirection: 'row', alignItems: 'center', gap: 12,
    flexBasis: '100%', flexGrow: 1,
  },
  statIconBox: { width: 40, height: 40, borderRadius: 12, alignItems: 'center', justifyContent: 'center' },
  statLabel: { color: COLORS.secondary, fontSize: 12, marginBottom: 4 },
  statValue: { color: COLORS.textMain, fontSize: 16, fontWeight: 'bold' },

  modalOverlay: {
    flex: 1, backgroundColor: 'rgba(0,0,0,0.5)',
    justifyContent: 'flex-end',
  },
  modalContent: {
    backgroundColor: COLORS.surface, borderTopLeftRadius: 24, borderTopRightRadius: 24,
    padding: SIZES.large, maxHeight: Dimensions.get('window').height * 0.7,
  },
  modalTitle: { fontSize: 18, fontWeight: 'bold', color: COLORS.textMain, marginBottom: 16 },
  modalList: { paddingBottom: SIZES.extraLarge },
  modalItem: { paddingVertical: 16, borderBottomWidth: 1, borderBottomColor: COLORS.border },
  modalItemSelected: { backgroundColor: `${COLORS.primary}20`, borderRadius: 8, paddingHorizontal: 16, borderBottomWidth: 0 },
  modalItemText: { fontSize: 16, color: COLORS.textMain },
  modalItemTextSelected: { color: COLORS.primary, fontWeight: 'bold' },
});
