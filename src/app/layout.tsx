import type { Metadata } from "next";
import { Inter, Arapey } from "next/font/google";
import "./globals.css";

const inter = Inter({ subsets: ["latin"], variable: "--font-inter" });
const arapey = Arapey({ weight: "400", subsets: ["latin"], variable: "--font-arapey" });

export const metadata: Metadata = {
  title: "GMS-100W | TDSC Engenharia",
  description: "Sistema de monitoramento de precisão",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="pt-BR">
      <body className={`${inter.variable} ${arapey.variable} font-sans bg-[#fff8ec] text-[#1b2440] antialiased`}>
        {children}
      </body>
    </html>
  );
}
