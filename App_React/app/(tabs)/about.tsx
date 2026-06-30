import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { COLORS } from '../../src/constants/theme';
import { BookOpen, Users, CheckCircle, Home, Activity, History, Settings, HardDrive, Server as ServerIcon, Cpu } from 'lucide-react-native';
import GlassCard from '../../src/components/GlassCard';

export default function AboutScreen() {
  return (
    <SafeAreaView style={styles.container} edges={['top']}>
      <View style={styles.header}>
        <Text style={styles.headerTitle}>Giới thiệu & Hướng dẫn</Text>
        <Text style={styles.headerSubtitle}>Khám phá các tính năng của ứng dụng</Text>
      </View>

      <ScrollView contentContainerStyle={styles.scrollContent} showsVerticalScrollIndicator={false}>

        {/* CARD 1: HƯỚNG DẪN */}
        <GlassCard style={styles.card}>
          <View style={styles.sectionHeader}>
            <View style={[styles.titleIconBox, { backgroundColor: 'rgba(0, 117, 255, 0.2)' }]}>
              <BookOpen size={22} color={COLORS.primary} />
            </View>
            <Text style={styles.sectionTitle}>Hướng dẫn sử dụng chi tiết</Text>
          </View>

          <View style={styles.guideItem}>
            <View style={[styles.iconBox, { backgroundColor: 'rgba(0, 117, 255, 0.15)' }]}>
              <Home size={20} color={COLORS.primary} />
            </View>
            <View style={styles.guideTextContainer}>
              <Text style={styles.guideTitle}>Dashboard</Text>
              <Text style={styles.guideDesc}>Hiển thị thống kê tổng quan về tình trạng kết nối mạng Mesh, số lượng thiết bị hoạt động và biểu đồ tóm tắt.</Text>
            </View>
          </View>

          <View style={styles.guideItem}>
            <View style={[styles.iconBox, { backgroundColor: 'rgba(1, 247, 167, 0.15)' }]}>
              <Cpu size={20} color={COLORS.success} />
            </View>
            <View style={styles.guideTextContainer}>
              <Text style={styles.guideTitle}>Nodes</Text>
              <Text style={styles.guideDesc}>Quản lý danh sách thiết bị Node, theo dõi trạng thái Online/Offline trực tiếp và nhận cảnh báo mất kết nối.</Text>
            </View>
          </View>

          <View style={styles.guideItem}>
            <View style={[styles.iconBox, { backgroundColor: 'rgba(138, 43, 226, 0.15)' }]}>
              <Activity size={20} color={COLORS.info} />
            </View>
            <View style={styles.guideTextContainer}>
              <Text style={styles.guideTitle}>Sensors</Text>
              <Text style={styles.guideDesc}>Giám sát thông số cảm biến môi trường qua các biểu đồ trực quan, cập nhật liên tục theo thời gian thực.</Text>
            </View>
          </View>

          <View style={styles.guideItem}>
            <View style={[styles.iconBox, { backgroundColor: 'rgba(255, 181, 71, 0.15)' }]}>
              <ServerIcon size={20} color={COLORS.warning} />
            </View>
            <View style={styles.guideTextContainer}>
              <Text style={styles.guideTitle}>Gateway</Text>
              <Text style={styles.guideDesc}>Giám sát tài nguyên phần cứng (Pin, sóng Wi-Fi, CPU, RAM) và trạng thái hoạt động của trạm trung tâm Gateway.</Text>
            </View>
          </View>

          <View style={styles.guideItem}>
            <View style={[styles.iconBox, { backgroundColor: 'rgba(255, 40, 92, 0.15)' }]}>
              <HardDrive size={20} color={COLORS.error} />
            </View>
            <View style={styles.guideTextContainer}>
              <Text style={styles.guideTitle}>Server Backend</Text>
              <Text style={styles.guideDesc}>Giám sát sức khỏe máy chủ đám mây (CPU, RAM, Uptime) và theo dõi tình trạng của hệ thống mạng WebSocket.</Text>
            </View>
          </View>

          <View style={styles.guideItem}>
            <View style={[styles.iconBox, { backgroundColor: 'rgba(45, 212, 191, 0.15)' }]}>
              <History size={20} color="#2dd4bf" />
            </View>
            <View style={styles.guideTextContainer}>
              <Text style={styles.guideTitle}>History & CSV</Text>
              <Text style={styles.guideDesc}>Lọc dữ liệu theo mốc thời gian, loại cảm biến để vẽ biểu đồ và hỗ trợ trích xuất báo cáo thành file CSV chia sẻ nội bộ.</Text>
            </View>
          </View>

          <View style={styles.guideItem}>
            <View style={[styles.iconBox, { backgroundColor: 'rgba(143, 167, 214, 0.15)' }]}>
              <Settings size={20} color={COLORS.secondary} />
            </View>
            <View style={styles.guideTextContainer}>
              <Text style={styles.guideTitle}>Settings</Text>
              <Text style={styles.guideDesc}>Tùy chỉnh các ngưỡng (Threshold) cảnh báo an toàn và thay đổi cấu hình kết nối máy chủ không cần build lại App.</Text>
            </View>
          </View>
        </GlassCard>

        {/* CARD 2: THÔNG TIN NHÓM */}
        <GlassCard style={styles.card}>
          <View style={styles.sectionHeader}>
            <View style={[styles.titleIconBox, { backgroundColor: 'rgba(1, 247, 167, 0.2)' }]}>
              <Users size={22} color={COLORS.success} />
            </View>
            <Text style={styles.sectionTitle}>Thông tin bài tập lớn & Nhóm</Text>
          </View>

          <View style={styles.infoRow}>
            <Text style={styles.infoLabel}>Môn học:</Text>
            <Text style={styles.infoValue}>Lập trình Web và Ứng dụng</Text>
          </View>
          <View style={styles.infoRow}>
            <Text style={styles.infoLabel}>Đề tài:</Text>
            <Text style={styles.infoValue}>HỆ THỐNG NHÚNG CHUẨN HÓA KẾT NỐI & QUẢN LÝ ĐA CẢM BIẾN</Text>
          </View>

          <View style={styles.divider} />

          <Text style={styles.teamTitle}>Bảng Phân công Chức năng:</Text>

          <View style={styles.memberBox}>
            <Text style={styles.memberName}>1. Hồ Xuân Phú (Nhóm trưởng)</Text>
            <Text style={styles.memberRole}>• Hạ tầng Backend (HTTP REST, WebSockets, Nginx){'\n'}• UI: Dashboard, Server, History (Truy vấn & CSV){'\n'}• Soạn thảo Báo cáo & Slide thuyết trình</Text>
            <View style={styles.statusRow}>
              <CheckCircle size={14} color={COLORS.success} />
              <Text style={styles.statusText}>Hoàn thành: 100%</Text>
            </View>
          </View>

          <View style={styles.memberBox}>
            <Text style={styles.memberName}>2. Hồ Quang Quân (Thành viên)</Text>
            <Text style={styles.memberRole}>• Thiết lập Hardware và cấu hình Mạng Mesh{'\n'}• UI: Nodes, Sensors Realtime, Gateway, Settings{'\n'}• Soạn thảo Báo cáo & Slide thuyết trình</Text>
            <View style={styles.statusRow}>
              <CheckCircle size={14} color={COLORS.success} />
              <Text style={styles.statusText}>Hoàn thành: 100%</Text>
            </View>
          </View>
        </GlassCard>

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
    paddingHorizontal: 20,
    paddingVertical: 20,
  },
  headerTitle: {
    fontSize: 28,
    fontWeight: 'bold',
    color: COLORS.textMain,
    marginBottom: 4,
  },
  headerSubtitle: {
    fontSize: 14,
    color: COLORS.textMuted,
  },
  scrollContent: {
    paddingHorizontal: 20,
    paddingBottom: 40,
  },
  card: {
    padding: 20,
    marginBottom: 30, // Khoảng cách xa giữa 2 card
  },
  sectionHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 20,
  },
  titleIconBox: {
    width: 36,
    height: 36,
    borderRadius: 12,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 12,
  },
  sectionTitle: {
    fontSize: 20,
    fontWeight: 'bold',
    color: COLORS.textMain,
  },
  guideItem: {
    flexDirection: 'row',
    marginBottom: 16,
  },
  iconBox: {
    width: 40,
    height: 40,
    borderRadius: 12,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 12,
    marginTop: 2,
  },
  guideTextContainer: {
    flex: 1,
  },
  guideTitle: {
    fontSize: 16,
    fontWeight: 'bold',
    color: COLORS.textMain,
    marginBottom: 4,
  },
  guideDesc: {
    fontSize: 14,
    color: COLORS.textMuted,
    lineHeight: 20,
  },
  infoRow: {
    flexDirection: 'row',
    marginBottom: 10,
  },
  infoLabel: {
    width: 80,
    fontSize: 14,
    color: COLORS.secondary,
    fontWeight: 'bold',
  },
  infoValue: {
    flex: 1,
    fontSize: 14,
    color: COLORS.textMain,
    lineHeight: 20,
  },
  divider: {
    height: 1,
    backgroundColor: COLORS.border,
    marginVertical: 16,
  },
  teamTitle: {
    fontSize: 16,
    fontWeight: 'bold',
    color: COLORS.textMain,
    marginBottom: 12,
  },
  memberBox: {
    backgroundColor: 'rgba(255,255,255,0.03)',
    padding: 16,
    borderRadius: 12,
    marginBottom: 12,
    borderWidth: 1,
    borderColor: 'rgba(255,255,255,0.05)',
  },
  memberName: {
    fontSize: 15,
    fontWeight: 'bold',
    color: COLORS.primary,
    marginBottom: 10,
  },
  memberRole: {
    fontSize: 13,
    color: COLORS.textMuted,
    marginBottom: 12,
    lineHeight: 20,
  },
  statusRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
  },
  statusText: {
    fontSize: 13,
    color: COLORS.success,
    fontWeight: 'bold',
  }
});
