import Svg, { Polyline } from 'react-native-svg';

import { colors } from '@/constants/theme';

export function MiniLineChart({ values, color = colors.primary }: { values: Array<number | null>; color?: string }) {
  const width = 280;
  const height = 72;
  const nums = values.filter((v): v is number => typeof v === 'number' && Number.isFinite(v));
  const min = nums.length ? Math.min(...nums) : 0;
  const max = nums.length ? Math.max(...nums) : 100;
  const span = max - min || 1;
  const step = values.length > 1 ? width / (values.length - 1) : width;
  const points = values
    .map((value, idx) => {
      if (value == null) return null;
      const y = height - ((value - min) / span) * (height - 8) - 4;
      return `${idx * step},${y}`;
    })
    .filter(Boolean)
    .join(' ');

  return (
    <Svg width="100%" height={height} viewBox={`0 0 ${width} ${height}`}>
      <Polyline points={points} fill="none" stroke={color} strokeLinecap="round" strokeLinejoin="round" strokeWidth={3} />
    </Svg>
  );
}
