import { DynamoDBClient, ScanCommand, UpdateItemCommand } from "@aws-sdk/client-dynamodb";
import { NextResponse } from "next/server";
import nodemailer from "nodemailer";

const client = new DynamoDBClient({ region: process.env.AWS_REGION || "us-east-1" });

const transporter = nodemailer.createTransport({
  host: process.env.SMTP_HOST || "smtp.gmail.com",
  port: parseInt(process.env.SMTP_PORT || "587"),
  secure: process.env.SMTP_PORT === "465",
  auth: {
    user: process.env.SMTP_USER,
    pass: process.env.SMTP_PASS,
  },
});

export async function POST(request: Request) {
  try {
    const { email } = await request.json();
    if (!email) return NextResponse.json({ error: "E-mail inválido." }, { status: 400 });

    // Busca cliente pelo E-mail. Numa estrutura maior, recomenda-se GSI (Global Secondary Index)
    const scanCmd = new ScanCommand({
      TableName: "Clientes_GMS",
      FilterExpression: "EmailDestino = :email",
      ExpressionAttributeValues: { ":email": { S: email } }
    });
    const res = await client.send(scanCmd);
    const userItem = res.Items?.[0];

    if (!userItem) {
      return NextResponse.json({ error: "E-mail não encontrado no sistema." }, { status: 404 });
    }

    const mac = userItem.mac?.S;
    if (!mac) return NextResponse.json({ error: "Erro de integridade do cliente." }, { status: 500 });

    const otp = Math.floor(100000 + Math.random() * 900000).toString();
    const expiry = new Date(Date.now() + 10 * 60000).toISOString(); // 10 min de validade

    // Salva o código temporariamente
    const updateCmd = new UpdateItemCommand({
      TableName: "Clientes_GMS",
      Key: { mac: { S: mac } },
      UpdateExpression: "SET otp_code = :otp, otp_expiry = :exp",
      ExpressionAttributeValues: {
        ":otp": { S: otp },
        ":exp": { S: expiry }
      }
    });
    await client.send(updateCmd);

    // Envio de E-mail via Nodemailer
    if (process.env.SMTP_USER) {
      await transporter.sendMail({
        from: `"TDSC Engenharia" <${process.env.SMTP_USER}>`,
        to: email,
        subject: "Seu código de acesso ao Cofre Térmico",
        html: `<div style="font-family:'Inter', sans-serif; background-color:#fff8ec; color:#1b2440; max-width:600px; margin:0 auto; padding:40px 20px; border-radius:16px; text-align:center; border: 1px solid #e5dfd5;">
          <h2 style="font-family:'Arapey', serif; color:#1b2440; font-size:28px; font-weight:normal; margin-bottom:10px;">Acesso Seguro</h2>
          <p style="font-size:14px; color:#4a5568; margin-bottom:30px;">O seu código de autorização exclusivo, válido por 10 minutos, está disponível abaixo.</p>
          <div style="background:#ffffff; color:#1b2440; padding:20px 40px; display:inline-block; border-radius:8px; font-size:36px; font-weight:600; letter-spacing:12px; margin:10px 0; border: 1px solid #e2e8f0; box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.05);">
            ${otp}
          </div>
          <p style="color:#a0aec0; font-size:12px; margin-top:40px;">TDSC Engenharia - Monitoramento de Alta Precisão</p>
          <p style="color:#a0aec0; font-size:11px;">Se não solicitou este código, por favor ignore esta mensagem.</p>
        </div>`
      });
    } else {
      console.warn("SMTP_USER não configurado. Código OTP gerado:", otp);
    }

    return NextResponse.json({ success: true, message: "Código enviado com sucesso!" });

  } catch (err: any) {
    console.error("Erro Send OTP:", err);
    return NextResponse.json({ error: "Erro interno no servidor." }, { status: 500 });
  }
}
