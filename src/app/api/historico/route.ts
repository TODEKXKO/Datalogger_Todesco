import { DynamoDBClient, QueryCommand, GetItemCommand } from "@aws-sdk/client-dynamodb";
import { unmarshall } from "@aws-sdk/util-dynamodb";
import { NextResponse } from "next/server";

const client = new DynamoDBClient({
  region: process.env.AWS_REGION || "us-east-1",
});

export const dynamic = 'force-dynamic';

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const mac = searchParams.get('mac');
    const diasStr = searchParams.get('dias');

    if (!mac || !diasStr) {
      return NextResponse.json({ error: "Parâmetros 'mac' e 'dias' são obrigatórios" }, { status: 400 });
    }

    const dias = parseInt(diasStr, 10);
    const dataCorte = new Date();
    dataCorte.setDate(dataCorte.getDate() - dias);
    const dataCorteISO = dataCorte.toISOString();

    // 1. Busca os dados de telemetria
    const command = new QueryCommand({
      TableName: "Telemetria_GMS",
      KeyConditionExpression: "mac = :macVal AND #ts >= :dataCorte",
      ExpressionAttributeNames: {
        "#ts": "timestamp"
      },
      ExpressionAttributeValues: {
        ":macVal": { S: mac },
        ":dataCorte": { S: dataCorteISO }
      },
      ScanIndexForward: false // Do mais recente para o mais antigo
    });

    const response = await client.send(command);
    const items = response.Items?.map(item => unmarshall(item)) || [];

    // 2. Busca informações do equipamento/cliente para o laudo RBC
    let nomeClinica = "Nome da Clínica Não Encontrado";
    let dataCalibracaoRbc = "Solicitar Atualização";
    let responsavelTecnico = "Responsável Não Informado";

    try {
      const clienteCommand = new GetItemCommand({
        TableName: "Clientes_GMS",
        Key: { mac: { S: mac } }
      });
      const clienteResponse = await client.send(clienteCommand);
      
      if (clienteResponse.Item) {
        const clienteData = unmarshall(clienteResponse.Item);
        if (clienteData.Nome_Clinica) nomeClinica = clienteData.Nome_Clinica;
        if (clienteData.Data_Calibracao_RBC) dataCalibracaoRbc = clienteData.Data_Calibracao_RBC;
        if (clienteData.Responsavel_Tecnico) responsavelTecnico = clienteData.Responsavel_Tecnico;
      }
    } catch (err) {
      console.error("Erro ao buscar dados do cliente Clientes_GMS, prosseguindo com fallback:", err);
    }

    return NextResponse.json({
      historico: items,
      cliente: { nomeClinica, dataCalibracaoRbc, responsavelTecnico }
    });

  } catch (error) {
    console.error("Erro ao consultar Histórico no DynamoDB:", error);
    console.log("Detalhes do erro:", JSON.stringify(error, null, 2));
    return NextResponse.json({ error: "Falha ao buscar dados históricos", details: (error as Error).message }, { status: 500 });
  }
}
