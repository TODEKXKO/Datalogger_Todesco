"use client";

import { useState } from "react";
import { useRouter } from "next/navigation";
import { Shield, ArrowRight, Loader2, KeyRound } from "lucide-react";

export default function Login() {
  const router = useRouter();
  const [email, setEmail] = useState("");
  const [code, setCode] = useState("");
  const [step, setStep] = useState<1 | 2>(1);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");

  const handleSendCode = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!email) return;
    setLoading(true);
    setError("");

    try {
      const res = await fetch("/api/auth/send", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ email })
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.error || "Erro ao enviar código de acesso.");
      
      setStep(2);
    } catch (err: any) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  const handleVerify = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!code) return;
    setLoading(true);
    setError("");

    try {
      const res = await fetch("/api/auth/verify", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ email, code })
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.error || "Código inválido.");
      
      router.push("/dashboard");
    } catch (err: any) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen flex items-center justify-center p-6 bg-[#fff8ec] relative overflow-hidden">
      <div className="w-full max-w-md relative z-10 space-y-8">
        <div className="text-center space-y-3">
          <div className="mx-auto w-16 h-16 bg-white border border-gray-100 rounded-2xl flex items-center justify-center mb-4 shadow-md shadow-gray-200/50">
            <Shield className="w-8 h-8 text-[#1b2440]" strokeWidth={1.5} />
          </div>
          <h1 className="text-4xl font-light tracking-tight text-[#1b2440] font-arapey">Cofre Térmico</h1>
          <p className="text-[#1b2440]/70 text-sm tracking-[0.2em] uppercase font-bold">Acesso Metrológico Seguro</p>
        </div>
        
        <div className="bg-white border border-gray-100 rounded-2xl p-8 shadow-xl shadow-gray-200/50">
          {error && (
            <div className="mb-6 p-4 rounded-xl bg-red-50 border border-red-100 text-base text-red-600 text-center font-medium">
              {error}
            </div>
          )}

          {step === 1 ? (
            <form onSubmit={handleSendCode} className="space-y-6">
              <div className="space-y-3">
                <label className="block text-sm font-bold text-[#1b2440]/70 uppercase tracking-widest">E-mail de Cadastro</label>
                <input 
                  type="email" 
                  value={email}
                  onChange={(e) => setEmail(e.target.value)}
                  placeholder="seu@email.com.br"
                  required
                  className="w-full bg-gray-50 border border-gray-200 rounded-xl px-5 py-4 text-base text-[#1b2440] placeholder:text-[#1b2440]/40 focus:outline-none focus:border-[#1b2440] focus:ring-1 focus:ring-[#1b2440] transition-all"
                />
              </div>
              <button 
                type="submit"
                disabled={loading}
                className="group relative w-full flex items-center justify-center gap-2 bg-[#1b2440] hover:bg-[#151c33] text-[#fff8ec] font-bold rounded-xl px-5 py-4 text-base transition-all shadow-md disabled:opacity-70"
              >
                {loading ? <Loader2 className="w-5 h-5 animate-spin" /> : "Acessar Cofre Térmico"}
                {!loading && <ArrowRight className="w-4 h-4 group-hover:translate-x-1 transition-transform" />}
              </button>
            </form>
          ) : (
            <form onSubmit={handleVerify} className="space-y-6">
              <div className="space-y-2 text-center mb-6">
                <p className="text-base text-[#1b2440]/70">Enviamos um código para o e-mail:<br/><span className="text-[#1b2440] font-bold mt-1 block">{email}</span></p>
              </div>
              <div className="space-y-3">
                <label className="block text-sm font-bold text-[#1b2440]/70 uppercase tracking-widest">Código de 6 dígitos</label>
                <div className="relative">
                  <KeyRound className="absolute left-4 top-1/2 -translate-y-1/2 w-6 h-6 text-[#1b2440]/50" />
                  <input 
                    type="text" 
                    value={code}
                    onChange={(e) => setCode(e.target.value.replace(/\D/g, '').slice(0, 6))}
                    placeholder="000000"
                    required
                    className="w-full bg-gray-50 border border-gray-200 rounded-xl pl-12 pr-5 py-4 text-center text-4xl tracking-[0.4em] font-bold text-[#1b2440] placeholder:text-[#1b2440]/20 focus:outline-none focus:border-[#1b2440] focus:ring-1 focus:ring-[#1b2440] transition-all"
                  />
                </div>
              </div>
              <button 
                type="submit"
                disabled={loading || code.length !== 6}
                className="w-full flex items-center justify-center gap-2 bg-[#1b2440] hover:bg-[#151c33] text-[#fff8ec] font-bold rounded-xl px-5 py-4 text-base transition-all shadow-md disabled:opacity-50 disabled:cursor-not-allowed"
              >
                {loading ? <Loader2 className="w-5 h-5 animate-spin" /> : "Validar e Entrar"}
              </button>
              <button 
                type="button"
                onClick={() => setStep(1)}
                className="w-full text-sm font-bold text-[#1b2440]/60 hover:text-[#1b2440] transition-colors"
              >
                Voltar e alterar e-mail
              </button>
            </form>
          )}
        </div>
      </div>
    </div>
  );
}
