# analisis_caudal.py
import pandas as pd
import numpy as np
import json

FILE_IN = "Datos_caudalimetros.xlsx"
THRESHOLD_LPM = 0.3
EXPORT_EVENTOS = "eventos_caudal.csv"
EXPORT_RESUMEN = "resumen_caudal.json"

df = pd.read_excel(FILE_IN)
df.columns = ["toma", "fecha", "hora", "agua_fria", "agua_caliente"]
df["timestamp"] = pd.to_datetime(df["fecha"].astype(str) + " " + df["hora"].astype(str), errors="coerce")
df = df.sort_values("timestamp").reset_index(drop=True)

for col in ["agua_fria", "agua_caliente"]:
    df[col] = pd.to_numeric(df[col], errors="coerce").fillna(0).clip(lower=0)

df["agua_total"] = df["agua_fria"] + df["agua_caliente"]
df["activo"] = (df["agua_total"] > THRESHOLD_LPM).astype(int)
cambios = df["activo"].diff().fillna(0)
evento_id = (cambios == 1).cumsum()
df["evento_id"] = np.where(df["activo"] == 1, evento_id, np.nan)

eventos = []
for eid, g in df.groupby("evento_id", dropna=True):
    g = g.sort_values("timestamp")
    t0 = g["timestamp"].iloc[0]
    tf = g["timestamp"].iloc[-1]
    dur_s = (tf - t0).total_seconds()
    mean_fria = g["agua_fria"].mean()
    mean_caliente = g["agua_caliente"].mean()
    mean_total = g["agua_total"].mean()
    vol_fria = mean_fria * dur_s / 60.0
    vol_caliente = mean_caliente * dur_s / 60.0
    vol_total = vol_fria + vol_caliente
    prop_caliente = (100.0 * vol_caliente / vol_total) if vol_total > 0 else 0.0

    eventos.append({
        "ID": int(eid),
        "Inicio": t0,
        "Fin": tf,
        "Duración_s": round(dur_s, 3),
        "Caudal_medio_Lmin": round(mean_total, 3),
        "Caudal_pico_Lmin": round(g["agua_total"].max(), 3),
        "Volumen_frio_L": round(vol_fria, 3),
        "Volumen_caliente_L": round(vol_caliente, 3),
        "Volumen_total_L": round(vol_total, 3),
        "Proporcion_caliente_%": round(prop_caliente, 2),
    })

eventos_df = pd.DataFrame(eventos).sort_values("ID")

resumen = {
    "n_eventos": int(len(eventos_df)),
    "volumen_total_frio_L": round(float(eventos_df["Volumen_frio_L"].sum()), 3) if len(eventos_df) else 0.0,
    "volumen_total_caliente_L": round(float(eventos_df["Volumen_caliente_L"].sum()), 3) if len(eventos_df) else 0.0,
    "volumen_total_L": round(float(eventos_df["Volumen_total_L"].sum()), 3) if len(eventos_df) else 0.0,
    "caudal_medio_global_Lmin": round(float(df["agua_total"].mean()), 3) if len(df) else 0.0,
    "umbral_Lmin": THRESHOLD_LPM,
}

eventos_df.to_csv(EXPORT_EVENTOS, index=False, encoding="utf-8")
with open(EXPORT_RESUMEN, "w", encoding="utf-8") as f:
    json.dump(resumen, f, ensure_ascii=False, indent=2, default=str)

print("Hecho. Exportados:", EXPORT_EVENTOS, "y", EXPORT_RESUMEN)
