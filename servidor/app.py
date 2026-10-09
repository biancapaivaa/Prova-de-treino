from flask import Flask, request, jsonify, render_template_string
import sqlite3
from pathlib import Path
from datetime import datetime

APP_DIR = Path(__file__).resolve().parent
DB_PATH = APP_DIR / "estufasmart.db"

app = Flask(__name__)

HTML = """
<!doctype html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>EstufaSmart</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body{font-family:Arial,sans-serif;background:#f4f7f4;margin:0;color:#1f2937}
    .wrap{max-width:1000px;margin:40px auto;padding:0 16px}
    h1{color:#157347}
    .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:16px}
    .card{background:#fff;border-radius:14px;padding:20px;box-shadow:0 4px 18px rgba(0,0,0,.08)}
    .big{font-size:2rem;font-weight:700}
    .state{font-weight:700}
    canvas{background:#fff;border-radius:14px;padding:12px;box-shadow:0 4px 18px rgba(0,0,0,.08);margin-top:16px}
    .muted{color:#6b7280}
  </style>
</head>
<body>
<div class="wrap">
  <h1>EstufaSmart</h1>
  <div class="grid">
    <div class="card">
      <div class="muted">Última temperatura</div>
      <div class="big" id="temp">-- °C</div>
    </div>
    <div class="card">
      <div class="muted">Umidade</div>
      <div class="big" id="umid">-- %</div>
    </div>
    <div class="card">
      <div class="muted">Estado atual</div>
      <div class="big state" id="estado">--</div>
    </div>
    <div class="card">
      <div class="muted">Janela / Luz</div>
      <div class="big" id="extra">--</div>
    </div>
  </div>

  <canvas id="grafico" height="110"></canvas>
</div>

<script>
let chart;

async function atualizar() {
  const r = await fetch('/api/dados?limit=60');
  const dados = await r.json();

  if (dados.length > 0) {
    const ultimo = dados[dados.length - 1];
    document.getElementById('temp').textContent = ultimo.temp.toFixed(1) + ' °C';
    document.getElementById('umid').textContent = ultimo.umid.toFixed(1) + ' %';
    document.getElementById('estado').textContent = ultimo.estado;
    document.getElementById('extra').textContent =
      (ultimo.janela ? 'ABERTA' : 'FECHADA') + ' / ' + ultimo.luz_pct.toFixed(0) + '%';
  }

  const labels = dados.map(x => x.recebido_em.slice(11,19));
  const temps = dados.map(x => x.temp);

  if (!chart) {
    chart = new Chart(document.getElementById('grafico'), {
      type: 'line',
      data: {
        labels: labels,
        datasets: [{ label: 'Temperatura (°C)', data: temps, tension: 0.25 }]
      },
      options: {
        responsive: true,
        animation: false,
        scales: { y: { suggestedMin: 15, suggestedMax: 35 } }
      }
    });
  } else {
    chart.data.labels = labels;
    chart.data.datasets[0].data = temps;
    chart.update();
  }
}

atualizar();
setInterval(atualizar, 5000);
</script>
</body>
</html>
"""

def conexao():
    con = sqlite3.connect(DB_PATH)
    con.row_factory = sqlite3.Row
    return con

def criar_banco():
    with conexao() as con:
        con.execute("""
            CREATE TABLE IF NOT EXISTS telemetria (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                placa TEXT NOT NULL,
                seq INTEGER NOT NULL,
                temp REAL NOT NULL,
                umid REAL NOT NULL,
                janela INTEGER NOT NULL,
                luz_pct REAL NOT NULL,
                estado TEXT NOT NULL,
                recebido_em TEXT NOT NULL
            )
        """)

def validar(payload):
    obrigatorios = ["device_id","placa","seq","temp","umid","janela","luz_pct","estado"]
    for c in obrigatorios:
        if c not in payload:
            return False, f"Campo obrigatório ausente: {c}"

    try:
        temp = float(payload["temp"])
        umid = float(payload["umid"])
        luz = float(payload["luz_pct"])
        seq = int(payload["seq"])
    except (TypeError, ValueError):
        return False, "temp, umid, luz_pct e seq precisam ser numéricos"

    if not (-10 <= temp <= 60):
        return False, "Temperatura fora do intervalo permitido (-10 a 60 °C)"
    if not (0 <= umid <= 100):
        return False, "Umidade fora do intervalo permitido (0 a 100%)"
    if not (0 <= luz <= 100):
        return False, "Luminosidade fora do intervalo permitido (0 a 100%)"
    if seq < 0:
        return False, "seq inválido"

    if isinstance(payload["janela"], bool):
        janela = payload["janela"]
    elif payload["janela"] in (0, 1, "0", "1"):
        janela = bool(int(payload["janela"]))
    else:
        return False, "janela deve ser true/false ou 0/1"

    estados = {"NORMAL","ATENCAO","CRITICO","RECONHECIDO","FALHA_SENSOR"}
    if str(payload["estado"]).upper() not in estados:
        return False, "Estado inválido"

    return True, {
        "device_id": str(payload["device_id"])[:80],
        "placa": str(payload["placa"])[:80],
        "seq": seq,
        "temp": temp,
        "umid": umid,
        "janela": 1 if janela else 0,
        "luz_pct": luz,
        "estado": str(payload["estado"]).upper()
    }

@app.get("/")
def painel():
    return render_template_string(HTML)

@app.post("/api/telemetria")
def receber_telemetria():
    payload = request.get_json(silent=True)
    if not isinstance(payload, dict):
        return jsonify({"ok": False, "erro": "Envie JSON válido"}), 400

    ok, resultado = validar(payload)
    if not ok:
        return jsonify({"ok": False, "erro": resultado}), 400

    d = resultado
    agora = datetime.now().isoformat(timespec="seconds")
    with conexao() as con:
        con.execute("""
            INSERT INTO telemetria
            (device_id, placa, seq, temp, umid, janela, luz_pct, estado, recebido_em)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            d["device_id"], d["placa"], d["seq"], d["temp"], d["umid"],
            d["janela"], d["luz_pct"], d["estado"], agora
        ))

    return jsonify({"ok": True}), 201

@app.get("/api/dados")
def dados():
    try:
        limit = max(1, min(int(request.args.get("limit", 60)), 500))
    except ValueError:
        limit = 60

    with conexao() as con:
        rows = con.execute("""
            SELECT device_id, placa, seq, temp, umid, janela, luz_pct, estado, recebido_em
            FROM telemetria
            ORDER BY id DESC
            LIMIT ?
        """, (limit,)).fetchall()

    itens = [dict(r) for r in reversed(rows)]
    for item in itens:
        item["janela"] = bool(item["janela"])
    return jsonify(itens)

@app.get("/api/ultima")
def ultima():
    with conexao() as con:
        row = con.execute("""
            SELECT device_id, placa, seq, temp, umid, janela, luz_pct, estado, recebido_em
            FROM telemetria
            ORDER BY id DESC
            LIMIT 1
        """).fetchone()

    if row is None:
        return jsonify({"ok": False, "erro": "Sem dados"}), 404

    item = dict(row)
    item["janela"] = bool(item["janela"])
    return jsonify(item)

if __name__ == "__main__":
    criar_banco()
    print("EstufaSmart rodando em http://0.0.0.0:5000")
    app.run(host="0.0.0.0", port=5000, debug=True)
