import { NextResponse } from 'next/server';
import type { NextRequest } from 'next/server';

export function middleware(request: NextRequest) {
  const session = request.cookies.get('auth_session');
  const path = request.nextUrl.pathname;

  // Rotas protegidas (todas dentro do dashboard e config)
  const isProtected = path.startsWith('/dashboard') || path.startsWith('/config');

  if (!session && isProtected) {
    return NextResponse.redirect(new URL('/login', request.url));
  }

  // Se já tem sessão, não precisa ver o login
  if (session && path === '/login') {
    return NextResponse.redirect(new URL('/dashboard', request.url));
  }

  return NextResponse.next();
}

export const config = {
  matcher: ['/dashboard/:path*', '/config/:path*', '/login'],
};
