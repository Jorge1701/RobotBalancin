let socket = io();

socket.on( 'data', ( data ) => {
	console.log( 'Recibido: ' + data );
} );

$( document ).ready( () => {
	$( '#girIzq' ).click( () => {
		socket.emit( 'gir', 'izq' );
	} );

	$( '#girDer' ).click( () => {
		socket.emit( 'gir', 'der' );
	} );
} );