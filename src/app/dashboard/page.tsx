"use client";

import { useEffect, useState } from "react";
import { NavBar } from "@/components/NavBar";
import { Download, Thermometer, Droplets, BatteryMedium, FileBadge, X } from "lucide-react";
import { LineChart, Line, XAxis, YAxis, Tooltip, ResponsiveContainer, CartesianGrid, ReferenceArea } from 'recharts';
import jsPDF from "jspdf";
import autoTable from "jspdf-autotable";
import SHA256 from "crypto-js/sha256";

const MAC_ADDRESS = "FC012CDA2F28";

export default function Dashboard() {
  const [data, setData] = useState<Record<string, unknown>[]>([]);
  const [latest, setLatest] = useState<{ temp?: number | string; umidade?: number | string; bateriaText?: string }>({});
  const [isModalOpen, setIsModalOpen] = useState(false);
  const [isGeneratingPDF, setIsGeneratingPDF] = useState(false);

  useEffect(() => {
    const fetchData = async () => {
      try {
        const response = await fetch('/api/telemetria');
        if (!response.ok) return;
        const json = await response.json();
        
        if (Array.isArray(json) && json.length > 0) {
          setData(json);
          const current = json[json.length - 1];
          
          let bateriaFormatada = '--%';
          let batValue = current.bat;
          if (batValue === undefined) batValue = current.bateria;
          if (batValue === undefined) batValue = current.battery;
          
          if (batValue !== undefined && batValue !== null) {
             const v = Number(batValue);
             if (v >= 4.5) {
                 bateriaFormatada = '100% 🔌 (Na Tomada)';
             } else {
                 const pct = Math.min(100, Math.max(0, ((v - 3.3) / (4.2 - 3.3)) * 100));
                 bateriaFormatada = `${Math.round(pct)}%`;
             }
          }

          setLatest({
            temp: current.temperatura ?? current.temp ?? '--',
            umidade: current.umidade ?? current.humidity ?? '--',
            bateriaText: bateriaFormatada,
          });
        }
      } catch (error) {
        console.error("Erro ao buscar telemetria", error);
      }
    };

    fetchData(); // Chamada inicial
    const interval = setInterval(fetchData, 10000); // Polling de 10s
    return () => clearInterval(interval);
  }, []);

  const formatDateTime = (tickItem: string | number) => {
    if (!tickItem) return '';
    try {
      let ts = String(tickItem);
      if (!ts.endsWith('Z')) ts += 'Z'; 
      const d = new Date(ts);
      return new Intl.DateTimeFormat('pt-BR', { 
        hour: '2-digit', 
        minute: '2-digit', 
        timeZone: 'America/Sao_Paulo' 
      }).format(d);
    } catch {
      return '';
    }
  };

  const handleGeneratePDF = async (dias: number, tituloPeriodo: string) => {
    try {
      setIsGeneratingPDF(true);
      const res = await fetch(`/api/historico?mac=${MAC_ADDRESS}&dias=${dias}`);
      if (!res.ok) throw new Error("Falha ao buscar histórico.");
      
      const payload = await res.json();
      const dadosHistorico = payload.historico || [];
      const cliente = payload.cliente || {
        nomeClinica: "Clínica Padrão",
        dataCalibracaoRbc: "Solicitar Atualização",
        responsavelTecnico: "Responsável Técnico"
      };

      if (!Array.isArray(dadosHistorico) || dadosHistorico.length === 0) {
        alert("Nenhum dado encontrado para o período selecionado.");
        setIsGeneratingPDF(false);
        return;
      }

      const cronologico = [...dadosHistorico].reverse();
      const filteredData: Record<string, unknown>[] = [];
      let lastSaved: { temp: number; time: number } | null = null;

      cronologico.forEach((item) => {
        const itemTemp = Number(item.temperatura ?? item.temp);
        let ts = item.timestamp || item.createdAt || "";
        if (ts && !ts.endsWith('Z')) ts += 'Z';
        const itemTime = new Date(ts).getTime();

        if (isNaN(itemTemp) || isNaN(itemTime)) return;

        if (!lastSaved) {
          filteredData.push(item);
          lastSaved = { temp: itemTemp, time: itemTime };
        } else {
          const tempDiff = Math.abs(itemTemp - lastSaved.temp);
          const timeDiffMinutes = (itemTime - lastSaved.time) / (1000 * 60);

          if (tempDiff > 0.5 || timeDiffMinutes >= 30) {
            filteredData.push(item);
            lastSaved = { temp: itemTemp, time: itemTime };
          }
        }
      });

      const hash = SHA256(JSON.stringify(filteredData)).toString();
      const doc = new jsPDF();
      
      doc.setFontSize(18);
      doc.setTextColor(27, 36, 64); // #1b2440
      doc.text(`TDSC Engenharia - ${cliente.nomeClinica}`, 14, 22);
      
      doc.setFontSize(9);
      doc.setTextColor(113, 113, 122); 
      doc.text(`Equipamento: GMS-100W | Sensor: SHT30 | Data da Calibração RBC: ${cliente.dataCalibracaoRbc}`, 14, 30);
      doc.text(`MAC: ${MAC_ADDRESS} | Período da Análise: ${tituloPeriodo} (${dias} ${dias === 1 ? 'dia' : 'dias'})`, 14, 35);
      doc.text(`Data de Geração: ${new Date().toLocaleString('pt-BR')}`, 14, 40);

      const tableData = filteredData.map((item: Record<string, unknown>) => {
        let ts = (item.timestamp || item.createdAt || "") as string;
        if (ts && !ts.endsWith('Z')) ts += 'Z';
        let dataStr = '--', horaStr = '--';
        if (ts) {
          const d = new Date(ts);
          dataStr = new Intl.DateTimeFormat('pt-BR', { timeZone: 'America/Sao_Paulo' }).format(d);
          horaStr = new Intl.DateTimeFormat('pt-BR', { hour: '2-digit', minute: '2-digit', timeZone: 'America/Sao_Paulo' }).format(d);
        }
        
        return [
          dataStr,
          horaStr,
          (item.temperatura ?? item.temp ?? '--') as string | number,
          (item.umidade ?? item.humidity ?? '--') as string | number
        ];
      });

      autoTable(doc, {
        startY: 48,
        head: [['Data', 'Hora', 'Temperatura °C', 'Umidade %']],
        body: tableData,
        theme: 'striped',
        headStyles: { fillColor: [27, 36, 64], textColor: [255, 255, 255], fontStyle: 'normal' },
        styles: { fontSize: 10, cellPadding: 4 },
        alternateRowStyles: { fillColor: [248, 250, 252] },
        didDrawPage: function () {
          doc.setFontSize(6);
          doc.setTextColor(161, 161, 170); 
          const pageHeight = doc.internal.pageSize.getHeight();
          doc.text(`Autenticidade Criptográfica (SHA-256): ${hash}`, 14, pageHeight - 10);
        }
      });

      const finalY = (doc as unknown as { lastAutoTable: { finalY: number } }).lastAutoTable?.finalY || 48;
      const pageHeight = doc.internal.pageSize.getHeight();
      
      let startAssinaturaY = finalY;
      if (finalY + 40 > pageHeight - 20) {
        doc.addPage();
        startAssinaturaY = 20;
      }
      
      const linhaY = startAssinaturaY + 30;
      doc.setDrawColor(27, 36, 64);
      doc.line(14, linhaY, 90, linhaY);
      
      doc.setFontSize(10);
      doc.setTextColor(27, 36, 64);
      doc.text(`${cliente.responsavelTecnico}`, 14, linhaY + 6);
      doc.setFontSize(8);
      doc.setTextColor(113, 113, 122);
      doc.text('Responsável Técnico / Legal pelo Estabelecimento', 14, linhaY + 11);

      const normalizedPeriodo = tituloPeriodo.normalize('NFD').replace(/[\u0300-\u036f]/g, '').replace(/ /g, '_');
      doc.save(`TDSC_Relatorio_${normalizedPeriodo}.pdf`);
      setIsModalOpen(false);
    } catch (error) {
      console.error(error);
      alert("Ocorreu um erro ao gerar o laudo.");
    } finally {
      setIsGeneratingPDF(false);
    }
  };

  return (
    <div className="min-h-screen bg-[#fff8ec] flex flex-col">
      <NavBar />
      <div className="flex-1 pt-28 pb-12 px-6 max-w-7xl mx-auto w-full space-y-8">
        {/* Header Luxury */}
        <div className="flex items-center justify-between">
          <div>
            <div className="flex items-center gap-4">
              <h1 className="text-4xl tracking-tight text-[#1b2440] font-arapey">Cofre Térmico</h1>
              <div className="flex items-center gap-2 mt-1 px-3 py-1 bg-white border border-[#e5dfd5] rounded-full shadow-sm">
                <span className="relative flex h-2 w-2">
                  <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75"></span>
                  <span className="relative inline-flex rounded-full h-2 w-2 bg-emerald-500"></span>
                </span>
                <span className="text-[10px] font-bold text-[#1b2440] uppercase tracking-wider">Operacional</span>
              </div>
            </div>
            <p className="text-xs text-[#1b2440]/50 tracking-widest mt-2 uppercase font-bold">ID: {MAC_ADDRESS}</p>
          </div>
          
          <div className="flex gap-3">
            <a 
              href={`https://tdsc-certificados-clinicas.s3.eu-north-1.amazonaws.com/${MAC_ADDRESS}.pdf`}
              target="_blank"
              rel="noopener noreferrer"
              download
              className="flex items-center gap-2 px-5 py-2.5 bg-white border border-[#e5dfd5] hover:border-[#1b2440]/30 hover:text-[#1b2440] rounded-xl text-sm font-bold text-[#1b2440]/70 transition-all duration-300 shadow-sm"
            >
              <FileBadge className="w-4 h-4" />
              Certificado RBC
            </a>
            <button 
              onClick={() => setIsModalOpen(true)}
              className="flex items-center gap-2 px-5 py-2.5 bg-[#1b2440] hover:bg-[#151c33] text-[#fff8ec] rounded-xl text-sm font-bold transition-colors shadow-md"
            >
              <Download className="w-4 h-4" />
              Exportar Laudo PDF
            </button>
          </div>
        </div>

        {/* KPI Cards */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
          <div className="bg-white border border-gray-100 rounded-2xl p-6 flex items-start justify-between shadow-md shadow-gray-200/50">
            <div>
              <p className="text-sm font-bold text-[#1b2440]/60 uppercase tracking-widest">Temperatura Atual</p>
              <p className="text-5xl font-semibold text-[#1b2440] mt-3">
                {latest.temp !== undefined ? latest.temp : '--'}
                {latest.temp !== '--' && <span className="text-2xl text-[#1b2440]/40 ml-1">°C</span>}
              </p>
            </div>
            <Thermometer className="w-8 h-8 text-[#1b2440]/30" />
          </div>
          <div className="bg-white border border-gray-100 rounded-2xl p-6 flex items-start justify-between shadow-md shadow-gray-200/50">
            <div>
              <p className="text-sm font-bold text-[#1b2440]/60 uppercase tracking-widest">Umidade</p>
              <p className="text-5xl font-semibold text-[#1b2440] mt-3">
                {latest.umidade !== undefined ? latest.umidade : '--'}
                {latest.umidade !== '--' && <span className="text-2xl text-[#1b2440]/40 ml-1">%</span>}
              </p>
            </div>
            <Droplets className="w-8 h-8 text-[#1b2440]/30" />
          </div>
          <div className="bg-white border border-gray-100 rounded-2xl p-6 flex items-start justify-between shadow-md shadow-gray-200/50">
            <div>
              <p className="text-sm font-bold text-[#1b2440]/60 uppercase tracking-widest">Nível de Bateria</p>
              <p className="text-3xl font-semibold text-[#1b2440] mt-5">
                {latest.bateriaText || '--%'}
              </p>
            </div>
            <BatteryMedium className="w-8 h-8 text-[#1b2440]/30" />
          </div>
        </div>

        {/* Chart Area */}
        <div className="bg-white border border-[#e5dfd5] rounded-[24px] p-8 h-[450px] flex flex-col shadow-sm">
          <p className="text-xs font-semibold text-[#1b2440]/50 uppercase tracking-wider mb-8">Monitoramento Contínuo</p>
          <div className="flex-1 w-full h-full">
            {data.length > 0 ? (
              <ResponsiveContainer width="100%" height="100%">
                <LineChart data={data} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                  <CartesianGrid strokeDasharray="3 3" stroke="#e5dfd5" vertical={false} />
                  <XAxis 
                    dataKey="timestamp" 
                    stroke="#1b2440" 
                    opacity={0.4}
                    fontSize={11} 
                    tickFormatter={formatDateTime} 
                    axisLine={false} 
                    tickLine={false} 
                    dy={10}
                  />
                  <YAxis 
                    stroke="#1b2440" 
                    opacity={0.4}
                    fontSize={11} 
                    axisLine={false} 
                    tickLine={false} 
                    domain={[0, 12]}
                    ticks={[0, 2, 4, 6, 8, 10, 12]}
                    dx={-10}
                  />
                  <Tooltip 
                    contentStyle={{ backgroundColor: '#ffffff', borderColor: '#e5dfd5', borderRadius: '12px', color: '#1b2440', boxShadow: '0 4px 6px -1px rgba(0, 0, 0, 0.05)' }}
                    itemStyle={{ color: '#1b2440', fontWeight: '500' }}
                    labelFormatter={formatDateTime}
                  />
                  
                  {/* Vermelho Alerta Hospitalar Leve: Áreas fora da conformidade RDC 430 (<2 e >8) */}
                  <ReferenceArea y2={2} fill="#ffe4e1" fillOpacity={0.4} />
                  <ReferenceArea y1={8} fill="#ffe4e1" fillOpacity={0.4} />

                  <Line 
                    type="monotone" 
                    dataKey={data[0]?.temperatura ? "temperatura" : "temp"} 
                    stroke="#1b2440" // Azul Marinho Premium
                    strokeWidth={2}
                    dot={false}
                    activeDot={{ r: 5, fill: "#fff8ec", stroke: "#1b2440", strokeWidth: 2 }}
                  />
                </LineChart>
              </ResponsiveContainer>
            ) : (
              <div className="h-full border border-dashed border-[#e5dfd5] rounded-xl flex items-center justify-center">
                <p className="text-[#1b2440]/40 text-sm animate-pulse">Aguardando telemetria do DynamoDB...</p>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Modal Elegante: Laudos PDF */}
      {isModalOpen && (
        <div className="fixed inset-0 z-[100] flex items-center justify-center bg-[#1b2440]/40 backdrop-blur-sm p-4">
          <div className="bg-white border border-[#e5dfd5] w-full max-w-sm rounded-[24px] p-8 shadow-2xl relative">
            <button 
              onClick={() => !isGeneratingPDF && setIsModalOpen(false)}
              disabled={isGeneratingPDF}
              className="absolute top-6 right-6 text-[#1b2440]/40 hover:text-[#1b2440] transition-colors bg-[#fcfaf7] rounded-full p-1 disabled:opacity-50"
            >
              <X className="w-5 h-5" />
            </button>
            <div className="mb-8">
              <h2 className="text-2xl text-[#1b2440] font-arapey">Exportar Laudo</h2>
              <p className="text-xs text-[#1b2440]/60 mt-2 font-medium">Selecione o período de análise técnica.</p>
            </div>
            
            <div className="space-y-3">
              <button 
                onClick={() => handleGeneratePDF(1, 'Diário')}
                disabled={isGeneratingPDF}
                className="w-full flex items-center justify-between p-4 rounded-xl border border-[#e5dfd5] bg-[#fcfaf7] hover:bg-white hover:border-[#1b2440]/20 hover:shadow-sm transition-all group disabled:opacity-50 disabled:cursor-not-allowed"
              >
                <span className="text-sm font-semibold text-[#1b2440] transition-colors">
                  {isGeneratingPDF ? 'Gerando...' : 'Relatório Diário'}
                </span>
                <Download className="w-4 h-4 text-[#1b2440]/40 group-hover:text-[#1b2440] transition-colors" />
              </button>
              
              <button 
                onClick={() => handleGeneratePDF(7, 'Semanal')}
                disabled={isGeneratingPDF}
                className="w-full flex items-center justify-between p-4 rounded-xl border border-[#e5dfd5] bg-[#fcfaf7] hover:bg-white hover:border-[#1b2440]/20 hover:shadow-sm transition-all group disabled:opacity-50 disabled:cursor-not-allowed"
              >
                <span className="text-sm font-semibold text-[#1b2440] transition-colors">
                  {isGeneratingPDF ? 'Gerando...' : 'Fechamento Semanal'}
                </span>
                <Download className="w-4 h-4 text-[#1b2440]/40 group-hover:text-[#1b2440] transition-colors" />
              </button>

              <button 
                onClick={() => handleGeneratePDF(30, 'Mensal')}
                disabled={isGeneratingPDF}
                className="w-full flex items-center justify-between p-4 rounded-xl border border-[#e5dfd5] bg-[#fcfaf7] hover:bg-white hover:border-[#1b2440]/20 hover:shadow-sm transition-all group disabled:opacity-50 disabled:cursor-not-allowed"
              >
                <span className="text-sm font-semibold text-[#1b2440] transition-colors">
                  {isGeneratingPDF ? 'Gerando...' : 'Relatório Mensal'}
                </span>
                <Download className="w-4 h-4 text-[#1b2440]/40 group-hover:text-[#1b2440] transition-colors" />
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
