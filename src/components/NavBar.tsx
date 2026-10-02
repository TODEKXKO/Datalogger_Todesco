import Link from 'next/link';

export function NavBar() {
  return (
    <nav className="fixed top-0 left-0 w-full border-b border-[#151c33] bg-[#1b2440] z-50 shadow-md">
      <div className="max-w-7xl mx-auto px-6 h-16 flex items-center justify-between">
        <div className="font-semibold tracking-wider text-sm uppercase text-[#fff8ec]">
          TDSC <span className="text-[#fff8ec]/70 font-normal">Engenharia</span>
        </div>
        <div className="flex gap-6 text-sm font-medium text-[#fff8ec]/80">
          <Link href="/dashboard" className="hover:text-white transition-colors">Dashboard</Link>
          <Link href="/" className="hover:text-white transition-colors">Configuração</Link>
          <a href="/api/auth/logout" className="hover:text-white transition-colors">Sair</a>
        </div>
      </div>
    </nav>
  );
}
