#include <iostream>
#include <conio.h>
#include <string>
#include <windows.h>
#include <vector>
#include <ctime>
#include <iomanip>
#include <map>
#include <cmath>

using namespace std;

// ==================== SERIAL CONTROLLER ====================
class SerialController {
private:
    HANDLE hSerial;
    bool connected;
    string accion;
    bool newDataAvailable;

public:
    SerialController() : hSerial(INVALID_HANDLE_VALUE), connected(false), newDataAvailable(false) {}

    bool connect(const char* portName) {
        hSerial = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hSerial == INVALID_HANDLE_VALUE) {
            return false;
        }

        DCB dcbSerialParams = {0};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        if (!GetCommState(hSerial, &dcbSerialParams)) {
            CloseHandle(hSerial);
            return false;
        }

        dcbSerialParams.BaudRate = CBR_9600;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;

        if (!SetCommState(hSerial, &dcbSerialParams)) {
            CloseHandle(hSerial);
            return false;
        }

        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        timeouts.WriteTotalTimeoutConstant = 50;
        timeouts.WriteTotalTimeoutMultiplier = 10;

        if (!SetCommTimeouts(hSerial, &timeouts)) {
            CloseHandle(hSerial);
            return false;
        }

        connected = true;
        clearSerialBuffer();
        return true;
    }

    void clearSerialBuffer() {
        if (!connected) return;
        DWORD errors;
        COMSTAT comStat;
        ClearCommError(hSerial, &errors, &comStat);
        if (comStat.cbInQue > 0) {
            vector<char> tmp(comStat.cbInQue + 1);
            DWORD read;
            ReadFile(hSerial, tmp.data(), tmp.size() - 1, &read, NULL);
        }
    }

    bool sendData(const string& data) {
        if (!connected) return false;
        DWORD bytesWritten;
        string dataWithNewline = data + "\n";
        return WriteFile(hSerial, dataWithNewline.c_str(), dataWithNewline.length(), &bytesWritten, NULL);
    }

    bool hasNewData() {
        if (!connected) return false;
        
        char buffer[256];
        DWORD bytesRead;
        if (ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            accion = buffer;
            // Limpiar saltos de línea
            while (!accion.empty() && (accion.back() == '\r' || accion.back() == '\n')) {
                accion.pop_back();
            }
            newDataAvailable = true;
            return true;
        }
        return false;
    }

    string getLastData() {
        newDataAvailable = false;
        return accion;
    }

    ~SerialController() {
        if (connected) {
            CloseHandle(hSerial);
        }
    }
};

// ==================== SISTEMA DE ESTACIONAMIENTO MEJORADO ====================
class Estacionamiento {
private:
    struct Lugar {
        string ticketId;
        bool ocupado;
        time_t horaEntrada;
    };
    
    vector<Lugar> lugares;
    map<string, int> ticketToLugar;
    int contadorTickets;
    int capacidad;
    const float tarifaPorHora = 20.0;
    
public:
    Estacionamiento(int cap) : capacidad(cap), contadorTickets(0) {
        lugares.resize(capacidad);
        for (int i = 0; i < capacidad; i++) {
            lugares[i] = {"", false, 0};
        }
    }
    
    string generarTicketId() {
        time_t ahora = time(nullptr);
        tm* tiempo = localtime(&ahora);
        contadorTickets++;
        
        ostringstream oss;
        oss << "TCK-" << setw(2) << setfill('0') << tiempo->tm_mday
            << setw(2) << setfill('0') << (tiempo->tm_mon + 1)
            << (tiempo->tm_year + 1900)
            << setw(4) << setfill('0') << contadorTickets;
        return oss.str();
    }
    
    int entrada() {
        for (int i = 0; i < capacidad; i++) {
            if (!lugares[i].ocupado) {
                string ticketId = generarTicketId();
                
                lugares[i].ticketId = ticketId;
                lugares[i].ocupado = true;
                lugares[i].horaEntrada = time(nullptr);
                
                ticketToLugar[ticketId] = i;
                
                //cout << "DEBUG: Entrada - Ticket " << ticketId << " en Lugar A-" << (i + 1) << endl;
                return i + 1;
            }
        }
        return -1;
    }

    // consulta ticket
    void consulta(const string& ticketId) {
        char buffer[80];

        // Buscar en el mapa
        auto it = ticketToLugar.find(ticketId);
        if (it != ticketToLugar.end()) {
            int lugarIndex = it->second;
            
            tm* fecha = localtime(&lugares[lugarIndex].horaEntrada);

            if (lugarIndex >= 0 && lugarIndex < capacidad && 
                lugares[lugarIndex].ocupado && 
                lugares[lugarIndex].ticketId == ticketId) {
                
                strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", fecha);

                // 5. Imprimir la cadena formateada usando `cout`
                cout << "La fecha y hora actuales son: " << buffer << endl;
                
                cout << "Hr. Entrada  = " << lugares[lugarIndex].horaEntrada << endl;
                cout << "Hr. Entrada  = " << fecha->tm_mday << "/" << fecha->tm_mon << "/" << fecha->tm_year << endl;
                cout << "     Ocupado = " << lugares[lugarIndex].ocupado << endl;
                cout << "    Ticket # = " << lugares[lugarIndex].ticketId << endl;
            }
        }
    }
    
    // Calcula el cobro basado en el tiempo transcurrido
    float calcularCobro(time_t horaEntrada, time_t horaSalida) {
        double diferenciaSegundos = difftime(horaSalida, horaEntrada);
        double minutos = diferenciaSegundos / 60.0;
        
        // Los primeros 15 minutos son gratis
        if (minutos <= 15) {
            return 0.0;
        }
        
        // Después de 15 minutos, se cobra por hora completa
        double horas = ceil(minutos / 60.0);
        return horas * tarifaPorHora;
    }
    
    // Salida con ticket específico y cálculo de cobro
    float salida(const string& ticketId) {
        //cout << "DEBUG: Intentando salida con ticket: " << ticketId << endl;
        
        // Buscar en el mapa
        auto it = ticketToLugar.find(ticketId);
        if (it != ticketToLugar.end()) {
            int lugarIndex = it->second;
            
            if (lugarIndex >= 0 && lugarIndex < capacidad && 
                lugares[lugarIndex].ocupado && 
                lugares[lugarIndex].ticketId == ticketId) {
                
                // Calcular cobro
                time_t horaSalida = time(nullptr);
                float cobro = calcularCobro(lugares[lugarIndex].horaEntrada, horaSalida);
                
                // Liberar el lugar
                lugares[lugarIndex].ocupado = false;
                string ticketLiberado = lugares[lugarIndex].ticketId;
                lugares[lugarIndex].ticketId = "";
                
                ticketToLugar.erase(it);
                
                /*cout << "DEBUG: Salida EXITOSA - Lugar A-" << (lugarIndex + 1) 
                     << " liberado. Ticket: " << ticketLiberado 
                     << " Cobro: $" << fixed << setprecision(2) << cobro << endl;*/
                
                return cobro;
            }
        }
        
        // Si no se encuentra en el mapa, buscar manualmente
        for (int i = 0; i < capacidad; i++) {
            if (lugares[i].ocupado && lugares[i].ticketId == ticketId) {
                // Calcular cobro
                time_t horaSalida = time(nullptr);
                float cobro = calcularCobro(lugares[i].horaEntrada, horaSalida);
                
                lugares[i].ocupado = false;
                string ticketLiberado = lugares[i].ticketId;
                lugares[i].ticketId = "";
                
                ticketToLugar.erase(ticketId);
                
                /*cout << "DEBUG: Salida MANUAL - Lugar A-" << (i + 1) 
                     << " liberado. Ticket: " << ticketLiberado 
                     << " Cobro: $" << fixed << setprecision(2) << cobro << endl;*/
                return cobro;
            }
        }
        
        //cout << "DEBUG: Salida FALLIDA - Ticket no encontrado: " << ticketId << endl;
        return -1.0f;
    }
    
    // Función de reparación de emergencia
    void repararInconsistencias() {
        cout << "DEBUG: Iniciando reparacion de inconsistencias..." << endl;
        
        // Reconstruir el mapa desde cero
        ticketToLugar.clear();
        int reparados = 0;
        
        for (int i = 0; i < capacidad; i++) {
            if (lugares[i].ocupado && !lugares[i].ticketId.empty()) {
                // Verificar si este ticket ya está en el mapa en otro lugar
                auto it = ticketToLugar.find(lugares[i].ticketId);
                if (it != ticketToLugar.end()) {
                    // ¡Inconsistencia! Dos lugares con el mismo ticket
                    cout << "DEBUG: Reparando inconsistencia - Ticket duplicado: " 
                         << lugares[i].ticketId << endl;
                    // Liberar el lugar actual (asumimos que es el incorrecto)
                    lugares[i].ocupado = false;
                    lugares[i].ticketId = "";
                } else {
                    // Agregar al mapa
                    ticketToLugar[lugares[i].ticketId] = i;
                    reparados++;
                }
            }
        }
        
        cout << "DEBUG: Reparacion completada. " << reparados << " tickets reconstruidos." << endl;
    }
    
    // Forzar liberación de un lugar específico
    bool forzarLiberacion(int numeroLugar) {
        if (numeroLugar < 1 || numeroLugar > capacidad) {
            return false;
        }
        
        int index = numeroLugar - 1;
        if (lugares[index].ocupado) {
            string ticketId = lugares[index].ticketId;
            lugares[index].ocupado = false;
            lugares[index].ticketId = "";
            
            // Eliminar del mapa si existe
            ticketToLugar.erase(ticketId);
            
            /*cout << "DEBUG: Liberación forzada - Lugar A-" << numeroLugar 
                 << " liberado. Ticket: " << ticketId << endl;*/
            return true;
        }
        return false;
    }
    
    void mostrarEstado() {
        system("cls");
        cout << "==========================================" << endl;
        cout << "    SISTEMA DE ESTACIONAMIENTO - v5.0" << endl;
        cout << "==========================================" << endl;
        
        int ocupados = 0;
        for (int i = 0; i < capacidad; i++) {
            cout << " A-" << (i + 1) << ": " 
                 << (lugares[i].ocupado ? "OCUPADO (" + lugares[i].ticketId + ")" : "LIBRE") 
                 << endl;
            if (lugares[i].ocupado) ocupados++;
        }
        
        cout << "------------------------------------------" << endl;
        cout << "           Estado: " << ocupados << "/" << capacidad << " ocupados" << endl;
        cout << " Contador tickets: " << contadorTickets << endl;
        cout << "     Mapa tickets: " << ticketToLugar.size() << " registros" << endl;
        cout <<           " Tarifa: $" << tarifaPorHora << " por hora" << endl;
        cout << "------------------------------------------" << endl;
        cout << "\n Comandos:" << endl;
        cout << "   E - Entrada vehiculo" << endl;
        cout << "   S - Salida vehiculo" << endl; 
        cout << "   I - Informacion de Ticket" << endl; 
        cout << "   D - Debug completo" << endl;
        cout << "   R - Reparar inconsistencias" << endl;
        cout << "   F - Forzar liberacion de lugar" << endl;
        cout << "   Q - Salir" << endl;
        cout << "==========================================" << endl;
    }
    
    void debugCompleto() {
        system("cls");
        cout << "=== DEBUG COMPLETO ===" << endl;
        cout << "Capacidad: " << capacidad << endl;
        cout << "Contador tickets: " << contadorTickets << endl;
        cout << "Tickets en mapa: " << ticketToLugar.size() << endl;
        cout << "Tarifa por hora: $" << tarifaPorHora << endl;
        cout << endl;
        
        cout << "ESTADO LUGARES:" << endl;
        for (int i = 0; i < capacidad; i++) {
            cout << "Lugar A-" << (i + 1) << ": ";
            if (lugares[i].ocupado) {
                cout << "OCUPADO por " << lugares[i].ticketId;
                // Calcular tiempo transcurrido
                time_t ahora = time(nullptr);
                double minutos = difftime(ahora, lugares[i].horaEntrada) / 60.0;
                cout << " (Tiempo: " << fixed << setprecision(1) << minutos << " min)";
                
                auto it = ticketToLugar.find(lugares[i].ticketId);
                if (it != ticketToLugar.end() && it->second == i) {
                    cout << " ✓ CONSISTENTE";
                } else {
                    cout << " ✗ INCONSISTENTE";
                }
            } else {
                cout << "LIBRE";
            }
            cout << endl;
        }
        
        cout << endl << "MAPA TICKETS:" << endl;
        for (const auto& pair : ticketToLugar) {
            cout << "  " << pair.first << " -> Lugar A-" << (pair.second + 1) << endl;
        }
        
        cout << endl << "Presione cualquier tecla para continuar...";
        _getch();
    }
    
    vector<string> getTicketsActivos() {
        vector<string> tickets;
        for (int i = 0; i < capacidad; i++) {
            if (lugares[i].ocupado) {
                tickets.push_back(lugares[i].ticketId + " (Lugar A-" + to_string(i + 1) + ")");
            }
        }
        return tickets;
    }

    // Función para crear timestamp exacto
    time_t crearTimestamp(int anio, int mes, int dia, int hora, int minuto, int segundo = 0) {
        tm tiempo = {};
        tiempo.tm_year = anio - 1900;  // Años desde 1900
        tiempo.tm_mon = mes - 1;       // Meses 0-11
        tiempo.tm_mday = dia;
        tiempo.tm_hour = hora;
        tiempo.tm_min = minuto;
        tiempo.tm_sec = segundo;
        tiempo.tm_isdst = -1;          // No considerar horario de verano
    
        return mktime(&tiempo);
    }

    void cargaPrevia(){
    
        // Crear los registros
        // 25/11/2025 20:08
        lugares[2].ticketId = "TCK-251120250005";
        lugares[2].ocupado = true;
        lugares[2].horaEntrada  = crearTimestamp(2025, 11, 25, 20, 8, 0);
        ticketToLugar["TCK-251120250005"] = 2;

        // 27/11/2025 10:06
        lugares[1].ticketId = "TCK-271120250003";
        lugares[1].ocupado = true;
        lugares[1].horaEntrada  = crearTimestamp(2025, 11, 27, 10, 6, 0);
        ticketToLugar["TCK-251120250003"] = 4;
    
        // 26/11/2025 22:28
        lugares[0].ticketId = "TCK-261120250001";
        lugares[0].ocupado = true;
        lugares[0].horaEntrada = crearTimestamp(2025, 11, 26, 22, 28, 0);
        ticketToLugar["TCK-271120250001"] = 5;

        contadorTickets = 5;
    }    

    // Función de formateo integrada
    string formatearCobro(double cantidad) {
        stringstream ss;
        ss.imbue(locale(""));  // Configuración regional
        ss << fixed << setprecision(2) << cantidad;
        return "$" + ss.str();
    }

};

// ==================== PROGRAMA PRINCIPAL MEJORADO ====================
int main() {
    Estacionamiento est(6);
    SerialController serial;
    string ultimoMensaje = "Sistema listo - v5.0";

    // Variables para controlar el modo de salida serial
    bool modoSalidaSerial = false;
    string ticketSalidaSerial = "";

    // Intentar conectar al puerto serial
    const char* puertos[] = {"COM3", "COM4", "COM5", "COM6", "COM7", "COM8"};
    bool conectado = false;
    for (const char* puerto : puertos) {
        if (serial.connect(puerto)) {
            conectado = true;
            ultimoMensaje = "Conectado a " + string(puerto);
            break;
        }
    }

    if (!conectado) {
        ultimoMensaje = "Modo simulacion (sin Arduino)";
    }

    est.cargaPrevia();

    while (true) {
        // Verificar datos seriales (excepto cuando estamos en medio de una salida serial)
        if (!modoSalidaSerial && serial.hasNewData()) {
            string dato = serial.getLastData();
            cout << "COMANDO RECIBIDO X SERIAL: " << dato << endl;

            // Extraer número del comando
            int comando = -1;
            size_t pos = dato.find_first_of("0123456789");
            if (pos != string::npos) {
                string numStr = dato.substr(pos, 2);
                try {
                    comando = stoi(numStr);
                } catch (...) {
                    comando = -1;
                }
            }

            if (comando == 40) { // Entrada
                int lugar = est.entrada();
                if (lugar != -1) {
                    serial.sendData("1"); // Éxito
                    ultimoMensaje = "Entrada automatica - Lugar A-" + to_string(lugar);
                } else {
                    serial.sendData("0"); // Fallo
                    ultimoMensaje = "ERROR\n Estacionamiento lleno!";
                }
            } 
            else if (comando == 30) { // Salida
                // Activar modo x sensores
                modoSalidaSerial = true;
                ultimoMensaje = "SALIDA: Ingrese ticket por consola...";
                cout << "\n=== MODO x SENSOR ACTIVADO ===" << endl;
                cout << "Ingrese el ticket para salida: ";
            }
            else {
                serial.sendData("0"); // Comando no reconocido
                ultimoMensaje = "Comando no reconocido: " + dato;
            }
            
            // Limpiar buffer después de procesar
            serial.clearSerialBuffer();
        }

        // Si estamos en modo salida serial, capturar el ticket por consola
        if (modoSalidaSerial) {
            if (_kbhit()) {
                char tecla = _getch();
                if (tecla == 27) { // ESC para cancelar
                    modoSalidaSerial = false;
                    serial.sendData("0"); // Cancelar salida
                    ultimoMensaje = "Salida cancelada";
                    cout << "\nSalida cancelada." << endl;
                } else if (tecla == '\r' || tecla == '\n') { // Enter
                    if (!ticketSalidaSerial.empty()) {
                        // Procesar salida con el ticket ingresado
                        float cobro = est.salida(ticketSalidaSerial);
                        if (cobro >= 0) {
                            if (cobro == 0) {
                                serial.sendData("1"); // Salida gratis
                                ultimoMensaje = "Salida \n    Ticket: " + ticketSalidaSerial + "  -  (GRATIS)";
                            } else {
                                serial.sendData("2"); // Salida con cobro
                                ultimoMensaje = "Salida - Ticket " + ticketSalidaSerial + " \n- Cobro: $" + est.formatearCobro(cobro);
                            }
                        } else {
                            serial.sendData("0"); // Error
                            ultimoMensaje = "ERROR: Ticket no encontrado  \n        " + ticketSalidaSerial;
                        }
                        ticketSalidaSerial.clear();
                        modoSalidaSerial = false;
                        cout << "\nProcesando salida..." << endl;
                        Sleep(1000);
                    }
                } else {
                    // Agregar carácter al ticket
                    ticketSalidaSerial += tecla;
                    cout << tecla; // Eco del carácter
                }
            }
        }

        // Mostrar interfaz y manejar teclado (solo si no estamos en modo salida serial)
        if (!modoSalidaSerial) {
            est.mostrarEstado();
            cout << "Ultima accion: " << ultimoMensaje << endl;
            cout << "==========================================" << endl;
            cout << "\nComando: ";

            if (_kbhit()) {
                char tecla = _getch();
                
                switch (toupper(tecla)) {
                    case 'E': {
                        int lugar = est.entrada();
                        if (lugar != -1) {
                            ultimoMensaje = "Entrada exitosa - Lugar A-" + to_string(lugar);
                            serial.sendData("1"); // Éxito
                        } else {
                            ultimoMensaje = "ERROR: Estacionamiento lleno!";
                            serial.sendData("0"); // error
                        }
                        break;
                    }
                    case 'I': {
                        cout << "\nIngrese ticket para consulta: ";
                        string ticketId;
                        cin >> ticketId;
                        cin.ignore(1000, '\n');
                        // aqui
                        est.consulta(ticketId);
                        cin.ignore(1000, '\n');
                        break;
                    }
                    case 'S': {
                        cout << "\nIngrese ticket para salida: ";
                        string ticketId;
                        cin >> ticketId;
                        cin.ignore(1000, '\n');
                        
                        float resultado = est.salida(ticketId);
                        if (resultado >= 0) {
                            ultimoMensaje = "Salida exitosa \n      Ticket: " + ticketId + "\n        Cobro: $" + est.formatearCobro (resultado);
                            serial.sendData("2"); // Éxito
                        } else {
                            ultimoMensaje = "ERROR: Ticket no encontrado - " + ticketId;
                            serial.sendData("0"); // error
                        }
                        break;
                    }
                    
                    case 'D': {
                        est.debugCompleto();
                        ultimoMensaje = "Debug completado";
                        break;
                    }
                    
                    case 'R': {
                        est.repararInconsistencias();
                        ultimoMensaje = "Reparacion de inconsistencias completada";
                        break;
                    }
                    
                    case 'F': {
                        cout << "\nIngrese numero de lugar a liberar (1-6): ";
                        int lugar;
                        cin >> lugar;
                        cin.ignore(1000, '\n');
                        
                        if (est.forzarLiberacion(lugar)) {
                            ultimoMensaje = "Lugar A-" + to_string(lugar) + " liberado forzadamente";
                        } else {
                            ultimoMensaje = "ERROR: No se pudo liberar el lugar A-" + to_string(lugar);
                        }
                        break;
                    }
                    
                    case 'Q': {
                        cout << "\nSaliendo del sistema..." << endl;
                        return 0;
                    }
                    
                    default: {
                        ultimoMensaje = "Tecla no reconocida";
                        serial.sendData("0"); // Error
                        break;
                    }
                }
            }
        } else {
            // Modo salida serial: mostrar pantalla especial
            system("cls");
            cout << "==========================================" << endl;
            cout << "       MODO AUTOMATICO ACTIVADO" << endl;
            cout << "==========================================" << endl;
            cout << " Ingrese el ticket para salida: " << ticketSalidaSerial << endl;
            cout << " Presione ESC para cancelar" << endl;
            cout << "==========================================" << endl;
        }
        
        Sleep(100);
    }
    
    return 0;
}