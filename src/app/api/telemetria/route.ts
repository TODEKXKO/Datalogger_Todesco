import { DynamoDBClient, QueryCommand } from "@aws-sdk/client-dynamodb";
import { unmarshall } from "@aws-sdk/util-dynamodb";
import { NextResponse } from "next/server";

// O SDK AWS carrega automaticamente AWS_ACCESS_KEY_ID e AWS_SECRET_ACCESS_KEY do ambiente
const client = new DynamoDBClient({
  region: process.env.AWS_REGION || "us-east-1",
});

export const dynamic = 'force-dynamic'; // Evita cache agressivo da API

export async function GET() {
  try {
    const command = new QueryCommand({
      TableName: "Telemetria_GMS",
      KeyConditionExpression: "mac = :macVal",
      ExpressionAttributeValues: {
        ":macVal": { S: "FC012CDA2F28" },
      },
      ScanIndexForward: false, // Se a tabela tiver um sort key (ex: timestamp), traz os mais recentes primeiro
      Limit: 50 // Limita para os últimos 50 registros para plotar o gráfico de forma limpa
    });

    const response = await client.send(command);
    
    // Converte de formato tipado do DynamoDB {"N": "25.5"} para JSON normal
    const items = response.Items?.map(item => unmarshall(item)) || [];
    
    // Inverte o array para ordem cronológica (mais antigo -> mais novo) para o gráfico
    const sortedItems = items.reverse();

    return NextResponse.json(sortedItems);
  } catch (error) {
    console.error("Erro ao consultar DynamoDB:", error);
    console.log("Detalhes do Erro da AWS:", JSON.stringify(error, null, 2));
    return NextResponse.json({ error: "Falha ao buscar dados", details: (error as Error).message }, { status: 500 });
  }
}
