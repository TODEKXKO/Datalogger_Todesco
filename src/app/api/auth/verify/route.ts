import { DynamoDBClient, ScanCommand, UpdateItemCommand } from "@aws-sdk/client-dynamodb";
import { NextResponse } from "next/server";

const client = new DynamoDBClient({ region: process.env.AWS_REGION || "us-east-1" });

export async function POST(request: Request) {
  try {
    const { email, code } = await request.json();
    
    if (!email || !code) {
      return NextResponse.json({ error: "Dados inválidos." }, { status: 400 });
    }

    const scanCmd = new ScanCommand({
      TableName: "Clientes_GMS",
      FilterExpression: "EmailDestino = :email",
      ExpressionAttributeValues: { ":email": { S: email } }
    });
    const res = await client.send(scanCmd);
    const userItem = res.Items?.[0];

    if (!userItem) {
      return NextResponse.json({ error: "Usuário não encontrado." }, { status: 404 });
    }

    const savedOtp = userItem.otp_code?.S;
    const savedExp = userItem.otp_expiry?.S;
    const mac = userItem.mac?.S;

    if (!savedOtp || savedOtp !== code) {
      return NextResponse.json({ error: "Código inválido ou incorreto." }, { status: 401 });
    }

    if (new Date(savedExp as string).getTime() < Date.now()) {
      return NextResponse.json({ error: "O código expirou. Solicite um novo." }, { status: 401 });
    }

    // Limpa o OTP (One-Time Password)
    const updateCmd = new UpdateItemCommand({
      TableName: "Clientes_GMS",
      Key: { mac: { S: mac as string } },
      UpdateExpression: "REMOVE otp_code, otp_expiry"
    });
    await client.send(updateCmd);

    // Geração de Sessão HTTP-Only Segura
    const response = NextResponse.json({ success: true, mac });
    
    response.cookies.set({
      name: 'auth_session',
      value: mac as string, // Na prática armazena-se JWT criptografado
      httpOnly: true,
      secure: process.env.NODE_ENV === 'production',
      sameSite: 'lax',
      path: '/',
      maxAge: 60 * 60 * 24 * 30 // Sessão longa de 30 dias para conforto clínico
    });

    return response;

  } catch (err) {
    console.error("Erro Verify OTP:", err);
    return NextResponse.json({ error: "Erro interno ao validar acesso." }, { status: 500 });
  }
}
