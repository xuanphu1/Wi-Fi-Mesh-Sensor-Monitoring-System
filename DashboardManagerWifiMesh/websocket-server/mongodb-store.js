const mongoose = require("mongoose");

function looksLikeMeshUdpJson(o) {
  return (
    o &&
    typeof o === "object" &&
    typeof o.v === "number" &&
    typeof o.n === "number" &&
    typeof o.i === "string" &&
    Array.isArray(o.p)
  );
}

function parsePacketTime(pkt, receivedAt) {
  const raw = pkt && typeof pkt.t === "string" ? pkt.t.trim() : "";
  if (!raw) {
    return { time: receivedAt, deviceTime: null, deviceRtc: "", nodeTimeValid: false };
  }

  const direct = new Date(raw);
  if (Number.isFinite(direct.getTime())) {
    return { time: direct, deviceTime: direct, deviceRtc: raw, nodeTimeValid: true };
  }

  const utcFallback = new Date(raw.endsWith("Z") ? raw : `${raw}Z`);
  if (Number.isFinite(utcFallback.getTime())) {
    return { time: utcFallback, deviceTime: utcFallback, deviceRtc: raw, nodeTimeValid: true };
  }

  return { time: receivedAt, deviceTime: null, deviceRtc: raw, nodeTimeValid: false };
}

function createMongoStore({ mongoUri, sensorValueFieldNames, sensorTypeName }) {
  const meshDataSchema = new mongoose.Schema(
    {
      ip: { type: String, required: true, index: true },
      deviceTime: { type: Date, default: null },
      serverReceivedTime: { type: Date, required: true, index: true },
      time: { type: Date, required: true, index: true },
      meshLevel: { type: Number, default: null },
      schemaVersion: { type: Number, default: null },
      packetLoss: { type: Number, default: null },
      port: { type: Number, default: null },
      sensorType: { type: Number, default: null },
      sensorName: { type: String, default: "" },
      field: { type: String, required: true },
      value: { type: Number, required: true },
      deviceRtc: { type: String, default: "" },
      nodeTimeValid: { type: Boolean, default: false },
      sourceIp: { type: String, default: "" },
    },
    { versionKey: false, collection: "Data" }
  );
  meshDataSchema.index({ ip: 1, field: 1, time: 1 });
  meshDataSchema.index({ time: -1 });

  const MeshData = mongoose.models.MeshData || mongoose.model("MeshData", meshDataSchema);

  function buildMeshRows(pkt, sourceIp, receivedAt) {
    if (!looksLikeMeshUdpJson(pkt)) return [];
    const parsedTime = parsePacketTime(pkt, receivedAt);
    const sampleTime = parsedTime.time;
    const meshLevel = Number(pkt.n);
    const schemaVersion = Number(pkt.v);
    const packetLossRaw = pkt.packetloss ?? pkt.packetLoss;
    const packetLoss =
      packetLossRaw !== undefined && packetLossRaw !== null && packetLossRaw !== ""
        ? Number(packetLossRaw)
        : null;
    const ip = String(pkt.i || "");
    if (!ip || ip === "0.0.0.0") return [];
    const rows = [];

    pkt.p.forEach((row) => {
      if (!Array.isArray(row) || row.length < 3) return;
      const port = Number(row[0]);
      const sensorType = Number(row[1]);
      const sensorName = sensorTypeName(sensorType);
      const values = row.slice(2);
      const labels = sensorValueFieldNames[sensorType] || [];
      values.forEach((cell, idx) => {
        const value = Number(cell);
        if (!Number.isFinite(value)) return;
        const field = labels[idx] || `v${idx}`;
        rows.push({
          ip,
          deviceTime: parsedTime.deviceTime,
          serverReceivedTime: receivedAt,
          time: sampleTime,
          meshLevel,
          schemaVersion,
          packetLoss: Number.isFinite(packetLoss) ? packetLoss : null,
          port,
          sensorType,
          sensorName,
          field,
          value,
          deviceRtc: parsedTime.deviceRtc,
          nodeTimeValid: parsedTime.nodeTimeValid,
          sourceIp,
        });
      });
    });

    if (Number.isFinite(packetLoss)) {
      rows.push({
        ip,
        deviceTime: parsedTime.deviceTime,
        serverReceivedTime: receivedAt,
        time: sampleTime,
        meshLevel,
        schemaVersion,
        packetLoss,
        port: 0,
        sensorType: -1,
        sensorName: "System",
        field: "packetloss",
        value: packetLoss,
        deviceRtc: parsedTime.deviceRtc,
        nodeTimeValid: parsedTime.nodeTimeValid,
        sourceIp,
      });
    }

    return rows;
  }

  async function connect() {
    await mongoose.connect(mongoUri);
  }

  async function persistMeshPacket(pkt, sourceIp, receivedAt = new Date()) {
    const rows = buildMeshRows(pkt, sourceIp, receivedAt);
    if (!rows.length) return 0;
    try {
      await MeshData.insertMany(rows, { ordered: false });
      return rows.length;
    } catch (err) {
      console.error("[mongo] insertMany failed:", err.message);
      return 0;
    }
  }

  async function getNodeSnapshot() {
    const rows = await MeshData.aggregate([
      { $sort: { time: -1 } },
      {
        $group: {
          _id: "$ip",
          lastSeen: { $first: "$time" },
          meshLevel: { $first: "$meshLevel" },
          schemaVersion: { $first: "$schemaVersion" },
          packetLoss: { $first: "$packetLoss" },
        },
      },
      { $sort: { _id: 1 } },
      {
        $project: {
          _id: 0,
          id: "$_id",
          ip: "$_id",
          lastSeen: 1,
          meshLevel: 1,
          schemaVersion: 1,
          packetLoss: 1,
        },
      },
    ]);
    return rows;
  }

  async function getHistoryIps(limit = 200) {
    const n = Math.max(1, Math.min(Number(limit) || 200, 2000));
    const rows = await MeshData.aggregate([
      { $sort: { time: -1 } },
      { $group: { _id: "$ip", lastSeen: { $first: "$time" } } },
      { $sort: { lastSeen: -1 } },
      { $limit: n },
      { $project: { _id: 0, ip: "$_id", lastSeen: 1 } },
    ]);
    return rows;
  }

  async function getHistorySeries({ ip, field, from, to, limit = 2000 }) {
    const ipNorm = String(ip || "").trim();
    const fieldNorm = String(field || "").trim();
    if (!ipNorm || !fieldNorm) return [];
    const max = Math.max(1, Math.min(Number(limit) || 2000, 5000));
    const filter = { ip: ipNorm, field: fieldNorm };
    if (from || to) {
      filter.time = {};
      if (from) {
        const d = new Date(from);
        if (Number.isFinite(d.getTime())) filter.time.$gte = d;
      }
      if (to) {
        const d = new Date(to);
        if (Number.isFinite(d.getTime())) filter.time.$lte = d;
      }
      if (Object.keys(filter.time).length === 0) delete filter.time;
    }
    return MeshData.find(filter)
      .sort({ time: 1 })
      .limit(max)
      .select("ip field time value -_id")
      .lean();
  }

  return {
    connect,
    persistMeshPacket,
    getNodeSnapshot,
    getHistoryIps,
    getHistorySeries,
    isMeshPayload: looksLikeMeshUdpJson,
  };
}

module.exports = {
  createMongoStore,
};
