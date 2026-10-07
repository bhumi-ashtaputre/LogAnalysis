from flask import Flask, render_template, jsonify
from pathlib import Path
import json

app = Flask(__name__)
ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results"

def load_json(name):
    path = RESULTS / name
    if not path.exists():
        return None
    with open(path, encoding="utf-8") as f:
        return json.load(f)

@app.route("/")
def index():
    result = load_json("sequential.json") or {}
    omp = load_json("openmp.json") or {}
    hybrid = load_json("hybrid.json") or {}
    return render_template("index.html", result=result, omp=omp, hybrid=hybrid)

@app.route("/api/result/<name>")
def api_result(name):
    if not name.endswith(".json"):
        name += ".json"
    data = load_json(name)
    if data is None:
        return jsonify({"error": "Result not found"}), 404
    return jsonify(data)

if __name__ == "__main__":
    RESULTS.mkdir(exist_ok=True)
    app.run(host="127.0.0.1", port=5000, debug=False)
