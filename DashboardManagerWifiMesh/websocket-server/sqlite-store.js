const sqlite3 = require("sqlite3");
const { open } = require("sqlite");

function looksLikeMeshUdpJson(o) {
  return (
    o &&
    typeof o === "object" &&
    typeof o.v === "number" &&
    typeof o.n === "number" &&
    (typeof o.i === "string" || typeof o.M === "string") &&
    Array.isArray(o.p)
  );
}

function createSqliteStore({ dbPath, sensorValueFieldNames, sensorTypeName }) {
  let db;

  function buildMeshRows(pkt, sourceIp, receivedAt) {
    if (!looksLikeMeshUdpJson(pkt)) return [];
    const serverTime = receivedAt.toISOString();
    const deviceTime = pkt && typeof pkt.t === "string" ? pkt.t.trim() : null;
    const meshLevel = Number(pkt.n);
    const schemaVersion = Number(pkt.v);
    const packetLossRaw = pkt.packetloss ?? pkt.packetLoss;
    const packetLoss =
      packetLossRaw !== undefined && packetLossRaw !== null && packetLossRaw !== ""
        ? Number(packetLossRaw)
        : null;
    const mac = String(pkt.M || pkt.i || "");
    if (!mac || mac === "0.0.0.0") return [];
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
          mac,
          deviceTime,
          serverTime,
          meshLevel,
          schemaVersion,
          packetLoss: Number.isFinite(packetLoss) ? packetLoss : null,
          port,
          sensorType,
          sensorName,
          field,
          value,
          nodeTimeValid: 0,
          sourceIp,
        });
      });
    });

    if (Number.isFinite(packetLoss)) {
      rows.push({
        mac,
        deviceTime,
        serverTime,
        meshLevel,
        schemaVersion,
        packetLoss,
        port: 0,
        sensorType: -1,
        sensorName: "System",
        field: "packetloss",
        value: packetLoss,
        nodeTimeValid: 0,
        sourceIp,
      });
    }

    return rows;
  }

  async function connect() {
    db = await open({
      filename: dbPath || "mesh-data.sqlite",
      driver: sqlite3.Database,
    });

    // Enable Write-Ahead Logging for better concurrency
    await db.exec("PRAGMA journal_mode = WAL;");

    await db.exec(`
      CREATE TABLE IF NOT EXISTS Data (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        mac TEXT NOT NULL,
        deviceTime TEXT,
        serverTime TEXT NOT NULL,
        meshLevel INTEGER,
        schemaVersion INTEGER,
        packetLoss REAL,
        port INTEGER,
        sensorType INTEGER,
        sensorName TEXT,
        field TEXT NOT NULL,
        value REAL NOT NULL,
        nodeTimeValid INTEGER,
        sourceIp TEXT
      );
      CREATE INDEX IF NOT EXISTS idx_data_time ON Data(serverTime DESC);
      CREATE INDEX IF NOT EXISTS idx_data_mac_field_time ON Data(mac, field, serverTime);
    `);
  }

  async function persistMeshPacket(pkt, sourceIp, receivedAt = new Date()) {
    const rows = buildMeshRows(pkt, sourceIp, receivedAt);
    if (!rows.length) return 0;

    const placeholders = rows.map(() => "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)").join(", ");
    const values = [];
    rows.forEach((r) => {
      values.push(
        r.mac,
        r.deviceTime,
        r.serverTime,
        r.meshLevel,
        r.schemaVersion,
        r.packetLoss,
        r.port,
        r.sensorType,
        r.sensorName,
        r.field,
        r.value,
        r.nodeTimeValid,
        r.sourceIp
      );
    });

    const query = `
      INSERT INTO Data (
        mac, deviceTime, serverTime, meshLevel, 
        schemaVersion, packetLoss, port, sensorType, sensorName, 
        field, value, nodeTimeValid, sourceIp
      ) VALUES ${placeholders}
    `;

    try {
      await db.run(query, values);
      return rows.length;
    } catch (err) {
      console.error("[sqlite] insert failed:", err.message);
      return 0;
    }
  }

  async function persistGatewayStatus(pkt, sourceIp, receivedAt = new Date()) {
    if (!pkt || pkt.type !== "gateway_status") return 0;
    const mac = pkt.mac || pkt.sta_mac || pkt.sta_ip || sourceIp || "gateway";
    const serverTime = receivedAt.toISOString();
    
    const metrics = {
      cpu_load: pkt.cpu_load_percent,
      ram_used: pkt.ram_used_percent,
      wifi_rssi: pkt.wifi_rssi,
      battery_v: pkt.battery_voltage_v,
      uptime_s: pkt.uptime_s
    };

    const rows = [];
    for (const [field, val] of Object.entries(metrics)) {
      const num = Number(val);
      if (Number.isFinite(num)) {
        rows.push({
          mac,
          deviceTime: null,
          serverTime,
          meshLevel: -1,
          schemaVersion: -1,
          packetLoss: null,
          port: -1,
          sensorType: -1,
          sensorName: "Gateway",
          field,
          value: num,
          nodeTimeValid: 0,
          sourceIp
        });
      }
    }

    if (!rows.length) return 0;

    const placeholders = rows.map(() => "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)").join(", ");
    const values = [];
    rows.forEach((r) => {
      values.push(
        r.mac, r.deviceTime, r.serverTime, r.meshLevel, r.schemaVersion,
        r.packetLoss, r.port, r.sensorType, r.sensorName, r.field,
        r.value, r.nodeTimeValid, r.sourceIp
      );
    });

    const query = `
      INSERT INTO Data (
        mac, deviceTime, serverTime, meshLevel, 
        schemaVersion, packetLoss, port, sensorType, 
        sensorName, field, value, nodeTimeValid, sourceIp
      )
      VALUES ${placeholders}
    `;

    try {
      await db.run(query, values);
      return rows.length;
    } catch (err) {
      console.error("[sqlite] persistGatewayStatus failed:", err.message);
      return 0;
    }
  }

  async function getNodeSnapshot() {
    const rows = await db.all(`
      SELECT 
        mac as id,
        mac,
        MAX(serverTime) as lastSeen,
        meshLevel,
        schemaVersion,
        packetLoss
      FROM Data
      GROUP BY mac
      ORDER BY mac ASC
    `);

    // SQLite returns ISO strings, we should try to match MongoDB output format if possible, 
    // but the frontend parses standard ISO dates anyway.
    return rows.map(r => ({
      ...r,
      lastSeen: new Date(r.lastSeen)
    }));
  }

  async function getHistoryMacs(limit = 200) {
    const n = Math.max(1, Math.min(Number(limit) || 200, 2000));
    const rows = await db.all(`
      SELECT mac, MAX(serverTime) as lastSeen, GROUP_CONCAT(DISTINCT sensorName) as sensors
      FROM Data
      GROUP BY mac
      ORDER BY lastSeen DESC
      LIMIT ?
    `, [n]);

    return rows.map(r => ({
      mac: r.mac,
      lastSeen: new Date(r.lastSeen),
      sensors: r.sensors ? r.sensors.split(",") : []
    }));
  }

  async function getHistorySeries({ mac, sensorName, field, from, to, limit = 2000 }) {
    const macNorm = String(mac || "").trim();
    const sensorNameNorm = String(sensorName || "").trim();
    const fieldNorm = String(field || "").trim();
    if (!macNorm || !fieldNorm) return [];
    const max = Math.max(1, Math.min(Number(limit) || 2000, 5000));

    let query = `SELECT mac, sensorName, field, serverTime as time, value FROM Data WHERE mac = ? AND field = ?`;
    const params = [macNorm, fieldNorm];

    if (sensorNameNorm) {
      query += ` AND sensorName = ?`;
      params.push(sensorNameNorm);
    }

    if (from) {
      const d = new Date(from);
      if (Number.isFinite(d.getTime())) {
        query += ` AND serverTime >= ?`;
        params.push(d.toISOString());
      }
    }
    if (to) {
      const d = new Date(to);
      if (Number.isFinite(d.getTime())) {
        query += ` AND serverTime <= ?`;
        params.push(d.toISOString());
      }
    }

    if (from) {
      query += ` ORDER BY serverTime ASC LIMIT ?`;
      params.push(max);
      const rows = await db.all(query, params);
      return rows.map(r => ({
        mac: r.mac,
        field: r.field,
        value: r.value,
        time: new Date(r.time)
      }));
    } else {
      query += ` ORDER BY serverTime DESC LIMIT ?`;
      params.push(max);
      const rows = await db.all(query, params);
      rows.reverse();
      return rows.map(r => ({
        mac: r.mac,
        field: r.field,
        value: r.value,
        time: new Date(r.time)
      }));
    }

  }

  function toLocalTimeStr(isoString) {
    if (!isoString) return "";
    const d = new Date(isoString);
    if (!Number.isFinite(d.getTime())) return isoString;
    const pad = n => String(n).padStart(2, '0');
    return `${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
  }

  async function getHistoryAllForMacCsv(mac, from, to, onProgress, res) {
    const macNorm = String(mac || "").trim();
    const header = "DeviceTime,ServerTime,MeshLevel,PacketLoss,Port,Sensor,Field,Value\n";
    
    if (!macNorm) {
      if (res) { res.write(header); res.end(); return; }
      return header;
    }

    let countQuery = `SELECT COUNT(*) as c FROM Data WHERE mac = ?`;
    let query = `SELECT deviceTime, serverTime, meshLevel, packetLoss, port, sensorName, field, value FROM Data WHERE mac = ?`;
    const params = [macNorm];

    if (from) {
      const d = new Date(from);
      if (Number.isFinite(d.getTime())) {
        countQuery += ` AND serverTime >= ?`;
        query += ` AND serverTime >= ?`;
        params.push(d.toISOString());
      }
    }
    if (to) {
      const d = new Date(to);
      if (Number.isFinite(d.getTime())) {
        countQuery += ` AND serverTime <= ?`;
        query += ` AND serverTime <= ?`;
        params.push(d.toISOString());
      }
    }

    const countRow = await db.get(countQuery, params);
    const total = countRow ? countRow.c : 0;

    query += ` ORDER BY serverTime ASC`;

    if (res) {
      res.write(header);
    }

    let csvAccumulator = res ? null : header;
    
    if (total === 0) {
      if (res) { res.end(); return; }
      return csvAccumulator;
    }

    const limit = 5000;
    let offset = 0;

    while (offset < total) {
      const chunkQuery = query + ` LIMIT ${limit} OFFSET ${offset}`;
      const rows = await db.all(chunkQuery, params);
      
      if (rows.length === 0) break;

      let chunkCsv = "";
      rows.forEach(r => {
        const dt = toLocalTimeStr(r.deviceTime);
        const st = toLocalTimeStr(r.serverTime);
        chunkCsv += `${dt},${st},${r.meshLevel || ""},${r.packetLoss || 0},${r.port || 0},${r.sensorName || ""},${r.field || ""},${r.value || 0}\n`;
      });

      if (res) {
        // Write to stream
        res.write(chunkCsv);
      } else {
        // Accumulate in memory
        csvAccumulator += chunkCsv;
      }

      offset += rows.length;
      if (onProgress) onProgress(offset, total);
    }

    if (res) {
      res.end();
      return;
    }
    return csvAccumulator;
  }

  return {
    connect,
    persistMeshPacket,
    persistGatewayStatus,
    getNodeSnapshot,
    getHistoryMacs,
    getHistorySeries,
    getHistoryAllForMacCsv,
    isMeshPayload: looksLikeMeshUdpJson,
  };
}

module.exports = {
  createSqliteStore,
};
