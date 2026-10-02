"use client";

import { useState } from "react";
import { NavBar } from "@/components/NavBar";
import { Bluetooth } from "lucide-react";

const SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const RX_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

export default function Config() {
  const [ssid, setSsid] = useState("");
  const [pass, setPass] = useState("");
  const [email, setEmail] = useState("");
  const [dadosEnviados, setDadosEnviados] = useState(false);
  const [statusMessage, setStatusMessage] = useState<{type: 'info' | 'success' | 'error', text: string} | null>(null);

  const handlePairing = async () => {
    try {
      if (!ssid) {
        setStatusMessage({ type: 'error', text: 'O Nome da Rede Wi-Fi é obrigatório.' });
        return;
      }

      setStatusMessage({ type: 'info', text: 'Buscando dispositivo...' });
      
      const device = await (navigator as any).bluetooth.requestDevice({
        filters: [{ namePrefix: "Todes" }, { namePrefix: "TDSC" }], // Adicionado TDSC conforme solicitado
        optionalServices: [SERVICE_UUID]
      });

      setStatusMessage({ type: 'info', text: 'Conectando ao dispositivo...' });
      const server = await device.gatt.connect();
      const service = await server.getPrimaryService(SERVICE_UUID);
      const characteristic = await service.getCharacteristic(RX_UUID);
      
      const payload = `${ssid};;${pass};;${email}`;
      const encoder = new TextEncoder();
      
      setStatusMessage({ type: 'info', text: 'Enviando configurações...' });
      setDadosEnviados(true);

      // Listener para desconexão (reboot da placa)
      device.addEventListener('gattserverdisconnected', () => {
        setStatusMessage({ type: 'success', text: '✅ Dados enviados! O aparelho está reiniciando e conectando ao Wi-Fi.' });
      });

      await characteristic.writeValue(encoder.encode(payload));
      
      // Se a promessa do writeValue resolver antes de desconectar
      setStatusMessage({ type: 'success', text: '✅ Dados enviados! O aparelho está reiniciando e conectando ao Wi-Fi.' });

    } catch (error: any) {
      console.error(error);
      if (dadosEnviados && error.name === 'NetworkError') {
        // Disconnect jogado como erro durante o envio (comum em ESP32 que reinicia)
        setStatusMessage({ type: 'success', text: '✅ Dados enviados! O aparelho está reiniciando e conectando ao Wi-Fi.' });
      } else {
        // Ignora erro se o usuário apenas fechou/cancelou o popup do bluetooth
        if (error.name !== 'NotFoundError') {
          setStatusMessage({ type: 'error', text: `Erro de conexão: ${error.message}` });
        } else {
          setStatusMessage(null);
        }
      }
    }
  };

  return (
    <div className="min-h-screen flex flex-col bg-[#fff8ec]">
      <NavBar />
      <div className="flex-1 flex items-center justify-center pt-24 pb-12 px-6">
        <div className="max-w-md w-full space-y-10 relative z-10">
          <div className="text-center space-y-3">
            <div className="mx-auto w-16 h-16 bg-white border border-[#e5dfd5] rounded-2xl flex items-center justify-center mb-6 shadow-md shadow-gray-200/50">
              <Bluetooth className="w-8 h-8 text-[#1b2440]" strokeWidth={1.5} />
            </div>
            <h1 className="text-4xl font-light tracking-tight text-[#1b2440] font-arapey">Configuração de Dispositivo</h1>
            <p className="text-[#1b2440]/70 text-sm tracking-[0.1em] uppercase font-bold">Pareamento e Sincronização Local</p>
          </div>

          <div className="space-y-6 bg-white p-8 rounded-[24px] border border-gray-100 shadow-xl shadow-gray-200/50">
            <div className="space-y-3">
              <label className="block text-sm font-bold text-[#1b2440]/70 uppercase tracking-widest">Nome da Rede Wi-Fi</label>
              <input 
                type="text" 
                value={ssid}
                onChange={(e) => setSsid(e.target.value)}
                placeholder="Ex: Clinica_Visitantes"
                className="w-full bg-gray-50 border border-gray-200 rounded-xl px-5 py-4 text-base text-[#1b2440] placeholder:text-[#1b2440]/30 focus:outline-none focus:border-[#1b2440] focus:ring-1 focus:ring-[#1b2440] transition-all"
              />
            </div>
            <div className="space-y-3">
              <label className="block text-sm font-bold text-[#1b2440]/70 uppercase tracking-widest">Senha da Rede</label>
              <input 
                type="password" 
                value={pass}
                onChange={(e) => setPass(e.target.value)}
                placeholder="Senha do Wi-Fi"
                className="w-full bg-gray-50 border border-gray-200 rounded-xl px-5 py-4 text-base text-[#1b2440] placeholder:text-[#1b2440]/30 focus:outline-none focus:border-[#1b2440] focus:ring-1 focus:ring-[#1b2440] transition-all"
              />
            </div>
            <div className="space-y-3">
              <label className="block text-sm font-bold text-[#1b2440]/70 uppercase tracking-widest">E-mail de Cadastro</label>
              <input 
                type="email" 
                value={email}
                onChange={(e) => setEmail(e.target.value)}
                placeholder="seu@email.com"
                className="w-full bg-gray-50 border border-gray-200 rounded-xl px-5 py-4 text-base text-[#1b2440] placeholder:text-[#1b2440]/30 focus:outline-none focus:border-[#1b2440] focus:ring-1 focus:ring-[#1b2440] transition-all"
              />
            </div>

            <button 
              onClick={handlePairing}
              className="w-full flex items-center justify-center gap-2 mt-4 bg-[#1b2440] hover:bg-[#151c33] text-[#fff8ec] font-bold rounded-xl px-5 py-4 text-base transition-all shadow-md"
            >
              <Bluetooth className="w-5 h-5" />
              Parear Datalogger via Bluetooth
            </button>

            {statusMessage && (
              <div className={`mt-4 p-4 rounded-xl text-base text-center border font-semibold ${
                statusMessage.type === 'error' ? 'bg-red-50 text-red-600 border-red-100' : 
                statusMessage.type === 'success' ? 'bg-emerald-50 text-emerald-600 border-emerald-100' : 
                'bg-blue-50 text-blue-600 border-blue-100'
              }`}>
                {statusMessage.text}
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
