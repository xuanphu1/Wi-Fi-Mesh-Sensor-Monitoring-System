import { Link } from 'expo-router';
import { StyleSheet, View, Text } from 'react-native';

import { COLORS, SIZES } from '@/constants/theme';

export default function NotFoundScreen() {
  return (
    <View style={styles.container}>
      <Text style={styles.title}>Không tìm thấy màn hình</Text>
      <Link href="/" style={styles.link}>
        Quay lại Dashboard
      </Link>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.background,
    alignItems: 'center',
    justifyContent: 'center',
    padding: SIZES.large,
  },
  title: {
    fontSize: SIZES.large,
    color: COLORS.textMain,
    fontWeight: 'bold',
  },
  link: {
    color: COLORS.primary,
    fontWeight: '700',
    marginTop: SIZES.medium,
  },
});
