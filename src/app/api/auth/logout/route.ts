import { NextResponse } from "next/server";

export async function GET(request: Request) {
  // Redireciona para a tela de login
  const url = new URL('/login', request.url);
  const response = NextResponse.redirect(url);
  
  // Apaga o cookie de sessão
  response.cookies.delete('auth_session');
  
  return response;
}
